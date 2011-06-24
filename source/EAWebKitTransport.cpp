/*
Copyright (C) 2008-2009 Electronic Arts, Inc.  All rights reserved.

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

/////////////////////////////////////////////////////////////////////////////
// EAWebKitTransport.cpp
// Created by Paul Pedriana
/////////////////////////////////////////////////////////////////////////////


#include <EAWebKit/EAWebKitTransport.h>
#include "ResourceHandleManager.h"
#include <EAWebKit/internal/EAWebKitAssert.h>

namespace EA
{
namespace WebKit
{

	static void WriteHeaderMapEntry(EA::WebKit::FixedString16& sKey, EA::WebKit::FixedString16& sValue, EA::WebKit::HeaderMap& headerMap)
	{
		while(!sKey.empty() && ((sKey.back() == ' ') || (sKey.back() == '\t')))
			sKey.pop_back(); // Remove trailing whitespace.

		while(!sValue.empty() && ((sValue.back() == ' ') || (sValue.back() == '\t')))
			sValue.pop_back(); // Remove trailing whitespace.

		if(!sKey.empty() && !sValue.empty())
		{
			const EA::WebKit::HeaderMap::value_type entry(sKey, sValue);
			headerMap.insert(entry);
		}
	}


/////////////////////////////////////////////////////////////////////////////
// TransportHandler
/////////////////////////////////////////////////////////////////////////////

EAWEBKIT_API void AddTransportHandler(TransportHandler* pTransportHandler, const char16_t* pScheme)
{
    WebCore::ResourceHandleManager* pRHM = WebCore::ResourceHandleManager::sharedInstance();
    EAW_ASSERT(pRHM);

    pRHM->AddTransportHandler(pTransportHandler, pScheme);
}


EAWEBKIT_API void RemoveTransportHandler(TransportHandler* pTransportHandler, const char16_t* pScheme)
{
    WebCore::ResourceHandleManager* pRHM = WebCore::ResourceHandleManager::sharedInstance();
    EAW_ASSERT(pRHM);

    pRHM->RemoveTransportHandler(pTransportHandler, pScheme);
}


EAWEBKIT_API TransportHandler* GetTransportHandler(const char16_t* pScheme)
{
    WebCore::ResourceHandleManager* pRHM = WebCore::ResourceHandleManager::sharedInstance();
    EAW_ASSERT(pRHM);

    return pRHM->GetTransportHandler(pScheme);
}


/////////////////////////////////////////////////////////////////////////////
// TransportInfo
/////////////////////////////////////////////////////////////////////////////

TransportInfo::TransportInfo()
  : mURI(),
    mEffectiveURI(),
  //mScheme(),
    mPort(0),
    mPath(),
    mHeaderMapOut(),
    mHeaderMapIn(),
    mCookieFilePath(),
    mResultCode(200),
    mPostSize(-1),
  //mMethod(),
  //mAuthorizationType()
    mAuthorizationRealm(),
    mTimeout(0),
    mTimeoutInterval(0),
    mbVerifyPeers(true),
    mbEffectiveURISet(false),
    mbProcessed(false),

    mpTransportServerJobInfo(NULL),
    mpRH(NULL),
    mpView(NULL),
    mpTransportServer(NULL),
    mpTransportHandler(NULL),
    mTransportHandlerData(0),
    mpCookieManager(NULL)
{
    mScheme[0] = 0;
    mMethod[0] = 0;
    mAuthorizationType[0] = 0;
}


/////////////////////////////////////////////////////////////////////////////
// TransportHandlerFile
/////////////////////////////////////////////////////////////////////////////

TransportHandlerFile::FileInfo::FileInfo()
  : mFileObject(EA::WebKit::FileSystem::kFileObjectInvalid),
    mFileSize(-1),
    mbRead(true)
{
}


TransportHandlerFile::TransportHandlerFile()
{
    #ifdef EA_DEBUG
        mJobCount = 0;
    #endif
}


TransportHandlerFile::~TransportHandlerFile()
{
    #ifdef EA_DEBUG
        EAW_ASSERT(mJobCount == 0); // If this fails then the TransportServer is leaking jobs.
    #endif
}


bool TransportHandlerFile::InitJob(TransportInfo* pTInfo, bool& bStateComplete)
{
    using namespace EA::WebKit;

    // We return true if we feel we can handle the job.
    FileSystem* pFS = GetFileSystem();

    if(pFS != NULL)
    {
        Allocator* pAllocator = GetAllocator();
        FileInfo*  pFileInfo  = new(pAllocator->Malloc(sizeof(FileInfo), 0, "EAWebKit/TransportHandlerFile")) FileInfo; 

        pTInfo->mTransportHandlerData = (uintptr_t)pFileInfo;
        bStateComplete = true;

        #ifdef EA_DEBUG
            mJobCount++;
        #endif

        return true;
    }

    return false;
}


bool TransportHandlerFile::ShutdownJob(TransportInfo* pTInfo, bool& bStateComplete)
{
    if(pTInfo != NULL && pTInfo->mTransportHandlerData)
    {
        Disconnect(pTInfo, bStateComplete);

        FileInfo*  pFileInfo  = (FileInfo*)pTInfo->mTransportHandlerData;
        Allocator* pAllocator = GetAllocator();

        pFileInfo->~FileInfo();
        pAllocator->Free(pFileInfo, sizeof(FileInfo));

        pTInfo->mTransportHandlerData = 0;

        #ifdef EA_DEBUG
            mJobCount--;
        #endif
    }

    bStateComplete = true;
    return true;
}


bool TransportHandlerFile::Connect(TransportInfo* pTInfo, bool& bStateComplete)
{
    using namespace EA::WebKit;

    bool bReturnValue = false;

    FileInfo* pFileInfo = (FileInfo*)pTInfo->mTransportHandlerData;
    EAW_ASSERT(pFileInfo != NULL);

    if(pFileInfo)
    {
        FileSystem* pFS = GetFileSystem();
        EAW_ASSERT(pFS != NULL);  // This should be non-NULL because InitJob found it to be non-NULL.

        pFileInfo->mFileObject = pFS->CreateFileObject();

        if(pFileInfo->mFileObject != FileSystem::kFileObjectInvalid)
        {
            pFileInfo->mbRead = strcmp(pTInfo->mMethod, "GET") == 0;

            int openFlags = pFileInfo->mbRead ? FileSystem::kRead : FileSystem::kWrite;

            if(pFS->OpenFile(pFileInfo->mFileObject, GET_FIXEDSTRING8(pTInfo->mPath)->c_str(), openFlags))
                bReturnValue = true;
            else
            {
                pFS->DestroyFileObject(pFileInfo->mFileObject);
                pFileInfo->mFileObject = FileSystem::kFileObjectInvalid;
            }
        }
    }

    bStateComplete = true;
    return bReturnValue;
}


bool TransportHandlerFile::Disconnect(TransportInfo* pTInfo, bool& bStateComplete)
{
    using namespace EA::WebKit;

    FileInfo* pFileInfo = (FileInfo*)pTInfo->mTransportHandlerData;
    EAW_ASSERT(pFileInfo != NULL);

    if(pFileInfo->mFileObject != FileSystem::kFileObjectInvalid)
    {
        FileSystem* pFS = GetFileSystem();
        EAW_ASSERT(pFS != NULL);

        if(pFS)
        {
            pFS->CloseFile(pFileInfo->mFileObject);
            pFS->DestroyFileObject(pFileInfo->mFileObject);
            pFileInfo->mFileObject = FileSystem::kFileObjectInvalid;
        }
    }

    bStateComplete = true;
    return true;
}


bool TransportHandlerFile::Transfer(TransportInfo* pTInfo, bool& bStateComplete)
{
    using namespace EA::WebKit;

    bool bResult = true;

    FileInfo* pFileInfo = (FileInfo*)pTInfo->mTransportHandlerData;
    EAW_ASSERT(pFileInfo != NULL);

    if(pFileInfo->mFileObject != FileSystem::kFileObjectInvalid)
    {
        FileSystem* pFS = GetFileSystem();
        EAW_ASSERT(pFS != NULL);

        if(pFileInfo->mbRead) // If we need to read a file from disk and send it to the TransportServer.
        {
            if(pFileInfo->mFileSize < 0) // If this is the first time through...
            {
                pFileInfo->mFileSize = pFS->GetFileSize(pFileInfo->mFileObject);
                pTInfo->mpTransportServer->SetExpectedLength(pTInfo, pFileInfo->mFileSize);

                // pTInfo->mpTransportServer->SetEncoding(pTInfo, char* pEncoding);
                // pTInfo->mpTransportServer->SetMimeType(pTInfo);
                // pTInfo->mpTransportServer->HeadersReceived(pTInfo);
            }

            // To consider: Enable reading more than just one chunk at a time. However, by doing 
            // so we could block the current thread for an undesirable period of time.

            const int64_t size = pFS->ReadFile(pFileInfo->mFileObject, pFileInfo->mBuffer, sizeof(pFileInfo->mBuffer));

            if(size >= 0) // If no error...
            {
                if(size > 0)
                    pTInfo->mpTransportServer->DataReceived(pTInfo, pFileInfo->mBuffer, size);
                else
                {
                    bStateComplete = true;
                    bResult        = true;
                }
            }
            else
            {
                bStateComplete = true;
                bResult        = false;
            }
        }
        else // Else we need to read a file from the TransportServer and write it to disk.
        {
            const int64_t size = pTInfo->mpTransportServer->ReadData(pTInfo, pFileInfo->mBuffer, sizeof(pFileInfo->mBuffer));

            if(size >= 0) // If no error...
            {
                if(size > 0)
                {
                    if(!pFS->WriteFile(pFileInfo->mFileObject, pFileInfo->mBuffer, size))
                    {
                        bStateComplete = true;
                        bResult        = false;
                    }
                }
                else
                {
                    bStateComplete = true;
                    bResult        = true;
                }
            }
            else
            {
                bStateComplete = true;
                bResult        = false;
            }
        }
    }
    else
    {
        bStateComplete = true;
        bResult        = false;
    }

    // For now, set it to either 200 (OK) or 404 (not found).
    if(bResult)
        pTInfo->mResultCode = 200;
    else
        pTInfo->mResultCode = 404;

    if(bStateComplete)
        pTInfo->mpTransportServer->DataDone(pTInfo, bResult);

    return true;
}

//Helper API for the transport handler
// Overwrites the entry if present, else creates it.
EAWEBKIT_API void SetHeaderMapValue(EASTLHeaderMapWrapper& headerMapWrapper, const char16_t* pKey, const char16_t* pValue)
{
	EA::WebKit::HeaderMap& headerMap = *GET_HEADERMAP(headerMapWrapper);
	EA::WebKit::HeaderMap::iterator itWK = headerMap.find_as(pKey, EA::WebKit::str_iless());

	if(itWK != headerMap.end())  // If the key is already present...
		itWK->second = pValue;   // Overwrite it with a new value...
	else
	{
		const EA::WebKit::HeaderMap::value_type entry(pKey, pValue);
		headerMap.insert(entry);
	}

}

EAWEBKIT_API const char16_t* GetHeaderMapValue(const EASTLHeaderMapWrapper& headerMapWrapper, const char16_t* pKey)
{
	EA::WebKit::HeaderMap& headerMap = *GET_HEADERMAP(headerMapWrapper);
	EA::WebKit::HeaderMap::iterator itWK = headerMap.find_as(pKey, EA::WebKit::str_iless());

	if(itWK != headerMap.end())  // If the key is present...
		return itWK->second.c_str();

	return 0;
}


EAWEBKIT_API void EraseHeaderMapValue(EASTLHeaderMapWrapper& headerMapWrapper, const char16_t* pKey)
{
	EA::WebKit::HeaderMap& headerMap = *GET_HEADERMAP(headerMapWrapper);
	headerMap.erase(pKey);
}


EAWEBKIT_API int32_t SetTextFromHeaderMapWrapper(const EASTLHeaderMapWrapper& headerMapWrapper, char* pHeaderMapText, uint32_t textCapacity)
{
	EAW_ASSERT_MSG(pHeaderMapText,"The buffer to copy the text is invalid. Please provide a buffer with at least 4096 bytes.");

	using namespace EA::WebKit;

	const EA::WebKit::HeaderMap& headerMap = *GET_HEADERMAP(headerMapWrapper);

	char*    p    = pHeaderMapText;
	char*    pEnd = pHeaderMapText + textCapacity - 1; // -1 because we want to have space for a trailing 0 char.
	uint32_t textSizeRequired = 0;

	for(HeaderMap::const_iterator it = headerMap.begin(); it != headerMap.end(); ++it)
	{
		const EA::WebKit::FixedString16& sKey   = it->first;
		const EA::WebKit::FixedString16& sValue = it->second;

		const int32_t availableSpace = (int32_t)(pEnd - p);
		const int32_t requiredSpace  = (int32_t)sKey.length() + 2 + (int32_t)sValue.length() + 2;  // blah: blah\r\n

		textSizeRequired += (uint32_t)requiredSpace;
		// We don't assert (availableSpace > requiredSpace) here. We let the caller do that.

		if(availableSpace > requiredSpace)  // If we can write this line...
		{
			// We assume the text is ASCII and not Unicode requiring UTF8 encoding.
			for(eastl_size_t i = 0, iEnd = sKey.size(); i != iEnd; i++)
				*p++ = (char)sKey[i];

			*p++ = ':';
			*p++ = ' ';

			for(eastl_size_t j = 0, jEnd = sValue.size(); j != jEnd; j++)
				*p++ = (char)sValue[j];

			*p++ = '\r';
			*p++ = '\n';
		}
		else
		{
            return(-1);
		}
	}

	if(textCapacity > 0) // If the user provided any capacity at all...
		*p = 0;          // then we will have enough space to write a nul to *p.

	return textSizeRequired;

}
// We read multi-line text like the following:
//    aaaa : bbbb\n
//    aaaa:    bbbb\n
//    aaaa:bb\n
//     bb\n
//    aaaa : bbbb\n

EAWEBKIT_API bool SetHeaderMapWrapperFromText(const char* pHeaderMapText, uint32_t textSize, EASTLHeaderMapWrapper& headerMapWrapper, bool bExpectFirstCommandLine, bool bClearMap)
{
	using namespace EA::WebKit;
	bool processedWithError = false;

	EA::WebKit::HeaderMap& headerMap = *GET_HEADERMAP(headerMapWrapper);

	enum Mode
	{
		kModeKey,               // We are in the process of reading the key
		kModeKeyValueSeparator, // We are in the process of reading the characters (e.g. " : ") between the key and the value.
		kModeValue,             // We are in the process of reading the value (which may span multiple lines).
		kModeNewLine            // We are in the process of reading the newline after a value.
	};

	Mode               mode = kModeKey;
	const char*        p    = pHeaderMapText;
	const char*        pEnd = pHeaderMapText + textSize;
	EA::WebKit::FixedString16      sKey;
	EA::WebKit::FixedString16      sValue;
	const eastl_size_t kMaxKeySize = 128;
	const eastl_size_t kMaxValueSize = 2048;
	bool               inErrorCondition = false;
	bool               colonFound = false;

	if(bClearMap)
		headerMap.clear();

	if(bExpectFirstCommandLine)
	{
		// The first line should always be the status line, without a key:value pair. So skip it.
		while(*p && (*p != '\r') && (*p != '\n') && (p < pEnd))
			++p;

		if(*p == '\r')
			++p;

		if(*p == '\n')
			++p;
	}

	if((*p == ' ') || (*p == '\t') || (*p == '\r') || (*p == '\n'))
	{
		processedWithError = true;
		return processedWithError;
	}

	while(*p && (p < pEnd))
	{
		char c = *p++;

		switch(mode)
		{
		case kModeKey:
			if((c == '\r') || (c == '\n'))  // If we have reached a newline before reading whitespace or ':' char, the header is malformed.
			{
				processedWithError = inErrorCondition = true;
				mode = kModeNewLine;
			}
			else if((c == ' ') || (c == '\t') || (c == ':'))
			{
				mode = kModeKeyValueSeparator;
				if(c == ':')
					colonFound = true;
			}
			else
			{
				if(sKey.length() < kMaxKeySize)
					sKey += (wchar_t)(c);
			}
			break;

		case kModeKeyValueSeparator:
			if((c == '\r') ||(c == '\n')) // If we have reached a newline before any chars of the value, the value is empty. 
			{
				inErrorCondition = !colonFound; // We are OK only if a colon was found between the key and the value.
				mode = kModeNewLine;
			}
			else if((c != ' ') && (c != '\t') && (c != ':')) // If it is part of the value...
			{
				sValue = (uint8_t)c;
				mode   = kModeValue;
			}
			else if(c == ':')
				colonFound = true;

			break;

		case kModeValue:
			if((c == '\r') || (c == '\n')) // If we reached the end of the line...
				mode = kModeNewLine;
			else
			{
				if(sValue.length() < kMaxValueSize)
					sValue += (uint8_t)c;
			}
			break;

		case kModeNewLine:
			{
				if((c != '\r') && (c != '\n')) // If we have reached the next line...
				{
					if((c == ' ') || (c == '\t'))  // If we have a value that is continued from the previous line...
					{
						mode = kModeValue; // Go back to appending to the value.

						// Eat leading whitespace.
						while((p < pEnd) && ((c = *p) == ' ') && (c == '\t'))
							p++;
					}
					else
					{
						// We beginning the new key/value pair. Finalize the previous one.
						if(!inErrorCondition) //Make sure that you are not in an error condition when writing this Key-Value pair
						{
							if((sKey.length() < kMaxKeySize) && (sValue.length() < kMaxValueSize)) // If no overflow occurred...
								WriteHeaderMapEntry(sKey, sValue, headerMap);
						}
						else
							processedWithError = true; //Also, indicate to the caller that you did encounter some error in processing.

						inErrorCondition = false;  // Reset the error condition for next line.
						mode = kModeKey;           // This is part of a new key.
						colonFound = false;
						sKey = (uint8_t)c;
						sValue.clear();
					}
				}

				break;
			}
		}
	}

	// Finalize the last entry.
	if(!inErrorCondition && (sKey.length() < kMaxKeySize) && (sValue.length() < kMaxValueSize)) // If no overflow occurred...
		WriteHeaderMapEntry(sKey, sValue, headerMap);

	return processedWithError;
}

} // namespace WebKit

} // namespace EA

