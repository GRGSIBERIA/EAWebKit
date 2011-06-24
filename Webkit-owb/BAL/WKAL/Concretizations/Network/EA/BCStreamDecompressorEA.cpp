/*
Copyright (C) 2010 Electronic Arts, Inc.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1.  Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
2.  Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
3.  Neither the name of Electronic Arts, Inc. ("EA") nor the names of
its contributors may be used to endorse or promote products derived
from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY ELECTRONIC ARTS AND ITS CONTRIBUTORS "AS IS" AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL ELECTRONIC ARTS OR ITS CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

///////////////////////////////////////////////////////////////////////////////
// EAWebKitUtilStreamDecompressor.cpp
//
// Copyright (c) 2010 Electronic Arts. All Rights Reserved.
// By Arpit Baldeva 
//
// Implementation for zlib, raw deflate and gzip specifications. 
//////////////////////////////////////////////////////////////////////////////

#include "StreamDecompressor.h"
#include <EAWebKit/EAWebKit.h>
#include <EAAssert/eaassert.h>

#ifdef BUILDING_EAWEBKIT_DLL
#include <zlib/zlib.h>
#else
#include <xceedzip/zlib.h>
#endif

#include <stdio.h>

//http://www.zlib.net/zlib_how.html provides documentation on how to use ZLib library. However, some parts of the documentation 
// are valid only for newer version of the Zlib(around 2005). Our is like from 1998.

//The gist is
//zlib wrapped compressed stream using DEFLATE algorithm - works flawless in our version.

//Raw compressed stream using DEFLATE algorithm - is detected and execution is adjusted accordingly.

//gzip wrapped compressed stream using DEFLATE algorithm - is detected and execution is adjusted accordingly. In our current version of 
//zlib, Z_STREAM_ERROR is thrown for the footer. We cheat a bit and use it to detect the footer and end the stream.

// The recommendation is to use the "deflate" as it uses less overhead. However, due to confusion surrounding around the word "deflate"
//gzip may be a better alternative.


//Smaller numbers fail inexplicably. Since we receive data in 1K chunks,we end up allocating a 16K buffer for decompression.
const int kDecompressionBufferFactor = 16;

#ifndef BUILDING_EAWEBKIT_DLL
extern EA::WebKit::IEAWebkit* gEAWebkitInstance;
#endif

extern "C" voidpf EAWEBKIT_ZLIB_ALLOC(voidpf pOpaque, uInt items, uInt size)
{
	return ACCESS_EAWEBKIT_API(GetAllocator())->Malloc(items * size, 0, 0);
}

extern "C" void EAWEBKIT_ZLIB_FREE(voidpf pOpaque, voidpf address)
{
	ACCESS_EAWEBKIT_API(GetAllocator())->Free(address, 0);
}

namespace EA
{
	namespace EAWEBKIT_PACKAGE_NAMESPACE
	{
		IStreamDecompressor::~IStreamDecompressor()
		{

		}

		int32_t IStreamDecompressor::Init()
		{
			return 0;
		}

		DeflateStreamDecompressor::DeflateStreamDecompressor(EA::EAWEBKIT_PACKAGE_NAMESPACE::eStreamType streamType)
			: mZStream(0)
			, mDecompressedDataCallback(0)
			, mDecompressionBuffer(0)
			, mDecompressionBufferCapacity(0)
			, mStreamType(streamType)
			, mUserData(0)
			, mInitialized(false)
			, mRawDeflateStream(false)
			, mCheckForErrorCondition(true)
		{
			
		}
		DeflateStreamDecompressor::~DeflateStreamDecompressor()
		{
			UnInit();
			
			ACCESS_EAWEBKIT_API(GetAllocator())->Free(mDecompressionBuffer,0);
			mDecompressionBuffer = 0;
		}

		// We need to UnInit in some cases because some servers, send the raw deflate stream for the "deflate" Content-Encoding.
		// We detect that situation and restart. This is transparent to the user. 
		void DeflateStreamDecompressor::UnInit()
		{
			inflateEnd(mZStream);
			
			ACCESS_EAWEBKIT_API(GetAllocator())->Free(mZStream,0);
			mZStream = 0;
			
			mStreamType = EA::EAWEBKIT_PACKAGE_NAMESPACE::eStreamTypeNone;
			mInitialized = false;
		}
		
		int32_t DeflateStreamDecompressor::Init()
		{
			EAW_ASSERT_MSG(!mInitialized, "Already Initialized"); //TODO: Change this for redirection
			EAW_ASSERT_MSG(mStreamType != EA::EAWEBKIT_PACKAGE_NAMESPACE::eStreamTypeNone, "Need correct stream type before it can initialize"); //TODO: Change this for redirection

			int status = 0;
			if(!mInitialized)
			{
				mZStream = (z_stream*)(ACCESS_EAWEBKIT_API(GetAllocator())->Malloc(sizeof(z_stream),0,0));

				mZStream->zalloc = (alloc_func)EAWEBKIT_ZLIB_ALLOC;
				mZStream->zfree = (free_func)EAWEBKIT_ZLIB_FREE;
				mZStream->opaque = Z_NULL;
				mZStream->next_in = Z_NULL;
				mZStream->avail_in = 0;

				switch(mStreamType)
				{
				case eStreamTypeZLib:
					status = inflateInit(mZStream);
					break;
				case eStreamTypeRaw:
					status = inflateInit2(mZStream,-MAX_WBITS);
					break;
				case eStreamTypeGZip:
					//Since we are using an old version of ZLib, the library lacks support of transparent initialization of such stream.
					//In newer versions, one can call inflateInit2(mZStream,-MAX_WBITS+32); to do this.
					//We expect the stream header to be removed at this point.
					status = inflateInit2(mZStream,-MAX_WBITS);
					break;
				default:
					EAW_ASSERT_MSG(false, "Unknown stream format"); 
				}
				

				if(status == Z_OK)
				{
					mInitialized = true;
				}
				else
				{
					char debugBuffer[64];
					sprintf(debugBuffer, "Error initializing decompressor - %d", status);
					EAW_ASSERT_MSG(false, debugBuffer);
				}
				
			}

			return status;

		}

		int32_t DeflateStreamDecompressor::Decompress(uint8_t* sourceStream, uint32_t sourceLength, uint8_t* decompressionBuffer, uint32_t decompressionBufferCapacity)
		{
			int status = 0;
			bool ignoreError = false;//We can intentionally ignore some errors.

			//If not already initialized, initialize at the first attempt to decompress.
			if(!mInitialized)
			{
				status = Init();
				if(status != Z_OK)
					return status;
			}

			//if the caller did not pass decompress buffer, allocate one. 
			if(!decompressionBuffer)
			{
				AllocateDecompressionBuffer(sourceLength);

				decompressionBuffer = mDecompressionBuffer;
				decompressionBufferCapacity = mDecompressionBufferCapacity;
			}
			
			mZStream->next_in = (Bytef*)sourceStream;
			mZStream->avail_in = sourceLength;
			
			int32_t totalBytesDecompressed = 0;
			char debugBuffer[128];
			while(mZStream->avail_in>0)
			{
				mZStream->next_out = (Bytef*)(decompressionBuffer);
				mZStream->avail_out = decompressionBufferCapacity;
				
				status = inflate(mZStream, Z_SYNC_FLUSH);

				int32_t bytesDecompressed = 0;
				switch(status)
				{
				case Z_OK: //Intentional fall through
				case Z_STREAM_END:
					bytesDecompressed = (decompressionBufferCapacity - mZStream->avail_out);
					totalBytesDecompressed += bytesDecompressed;
					if(bytesDecompressed && mDecompressedDataCallback)
						mDecompressedDataCallback(mUserData, decompressionBuffer, bytesDecompressed);
					
					if(status == Z_STREAM_END)
						status = inflateEnd(mZStream);
					
					break;

				case Z_DATA_ERROR:
					if(mCheckForErrorCondition) //It could be a raw compressed stream or gzip stream with header data, try again
					{
						mCheckForErrorCondition = false;//So that we don't end up in a endless loop in case of an error
						if(mStreamType == eStreamTypeZLib)
						{
							UnInit();
							mStreamType = eStreamTypeRaw;
							status = Init();
							if(status != Z_OK)
								break;

							mZStream->next_in = (Bytef*)sourceStream;
							mZStream->avail_in = sourceLength;
							continue;

						}
						else if (mStreamType == eStreamTypeGZip)
						{
							UnInit();
							mStreamType = eStreamTypeGZip;
							status = Init();
							if(status != Z_OK)
								break;
							
							status = ProcessGZipHeader(sourceStream,sourceLength);
							if(status != Z_OK)
							{
								EAW_ASSERT_MSG(false, "Error processing GZip Header");
								break;
							}
							
							mZStream->next_in = (Bytef*)sourceStream;
							mZStream->avail_in = sourceLength;
							continue;
							
						}
					}
					break;
				case Z_STREAM_ERROR:
					//inflate() can also return Z_STREAM_ERROR, which should not be possible here, but could be checked for as noted above for def(). 
					//Z_BUF_ERROR does not need to be checked for here, for the same reasons noted for def(). Z_STREAM_END will be checked for later.
					if(mStreamType == eStreamTypeGZip)
					{
						ignoreError = true;
					}
					else
					{
						sprintf(debugBuffer, "Error in decompressor - %d", status);
						EAW_ASSERT_MSG(false, debugBuffer);
					}
					
					break;
				default:
					sprintf(debugBuffer, "Unknown Error - %d", status);
					EAW_ASSERT_MSG(false, debugBuffer);
					break;
				}
				
				//Catch all errors here
				if(status != Z_OK)
					break;
			} //While Loop
			
			if(status <0)
			{
				inflateEnd(mZStream);
				
				if(ignoreError)
					return Z_OK;
				else
					return status;
			}
			
			return totalBytesDecompressed;
		}

		
		void DeflateStreamDecompressor::SetDecompressedDataCallback(DecompressedDataCallback callback, void* userData)
		{
			mDecompressedDataCallback = callback;
			mUserData = userData;
		}

		int32_t DeflateStreamDecompressor::ProcessGZipHeader(uint8_t*& sourceStream, uint32_t& sourceLength)
		{
			int status = 0;
			//GZip header info is here - http://tools.ietf.org/html/rfc1952
			
			//Return -1 to indicate any error.
			
			//Need at least 10 bytes
			if(sourceLength<10) 
			{
				EAW_ASSERT_MSG(false, "GZip Incomplete header error");
				return -1;
			}
			
			//Check for the magic bytes that identify the GZip stream
			if((*(sourceStream++) != 0x1f) || (*(sourceStream++) != 0x8b))
			{
				EAW_ASSERT_MSG(false, "GZip file format detection error");
				return -1;
			}
			
			//Check for the method
			if(*(sourceStream++) != Z_DEFLATED)
			{
				EAW_ASSERT_MSG(false, "GZip Unknown scheme error");
				return -1;
			}
			
			//save the flag byte as it would be required later
			uint8_t flagByte = *(sourceStream++);

			//Check for the reserved bits
			if((flagByte & 0xE0) != 0)
			{
				EAW_ASSERT_MSG(false, "GZip reserved bits not zero error");
				return -1;
			}
			
			// don't care about the rest of the bytes
			sourceStream += 6;
			sourceLength -= 10; //4 + 6

			//We are done with the regular header at this moment.
			//Now we need to process any extra stuff from the the flag byte that we saved.

			if(flagByte & 0x04)//FLG.FEXTRA
			{
				if(sourceLength<2) //Make sure we have the 2 bytes
				{
					EAW_ASSERT_MSG(false, "GZip insufficient length error 1");
					return -1;
				}
				
				uint8_t byte1 = *(sourceStream++);
				uint8_t byte2 = *(sourceStream++);
				sourceLength -=2;

				uint32_t extraLen = ((byte2 << 8) | byte1);
				
				if(sourceLength<extraLen)//Make sure we have the extraLen bytes
				{
					EAW_ASSERT_MSG(false, "GZip insufficient length error 2");
					return -1;
				}
				
				sourceStream += extraLen;
				sourceLength -= extraLen; 
			}

			//check for the file name
			if(flagByte & 0x08) //FLG.FNAME
			{
				while(*(sourceStream++))
				{
					--sourceLength;
					if(sourceLength == 0)
					{
						EAW_ASSERT_MSG(false, "GZip insufficient length error 3");
						return -1;
					}
				}
				--sourceLength;//Adjust for the NULL termination
			}

			//Check for comment
			if(flagByte & 0x10)//FLG.FCOMMENT
			{
				while(*(sourceStream++))
				{
					--sourceLength;
					if(sourceLength == 0)
					{
						EAW_ASSERT_MSG(false, "GZip insufficient length error 4");
						return -1;
					}
				}
				--sourceLength;//Adjust for the NULL termination
			}
			
			//Get rid of the CRC bytes
			if(flagByte & 0x02)//FLG.FHCRC
			{
				if(sourceLength<2) //Make sure we have the 2 bytes
				{
					EAW_ASSERT_MSG(false, "GZip insufficient length error 5");
					return -1;
				}
				
				sourceStream +=2;
				sourceLength -=2;
			}
			return status;
		}

		void DeflateStreamDecompressor::AllocateDecompressionBuffer( uint32_t sourceLength )
		{
			if(!mDecompressionBuffer)
			{
				mDecompressionBufferCapacity = kDecompressionBufferFactor * sourceLength;
				mDecompressionBuffer = (uint8_t*)(ACCESS_EAWEBKIT_API(GetAllocator())->Malloc(mDecompressionBufferCapacity, 0, 0));
			}
			else
			{
				if(mDecompressionBufferCapacity < (kDecompressionBufferFactor*sourceLength))
				{
					ACCESS_EAWEBKIT_API(GetAllocator())->Free(mDecompressionBuffer, 0);
					mDecompressionBufferCapacity = kDecompressionBufferFactor * sourceLength;
					mDecompressionBuffer = (uint8_t*) (ACCESS_EAWEBKIT_API(GetAllocator())->Malloc(mDecompressionBufferCapacity, 0, 0));
				}
			}
		}

	}
	
}

