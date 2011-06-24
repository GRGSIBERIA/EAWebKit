/*
Copyright (C) 2009 Electronic Arts, Inc.  All rights reserved.

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
// BCTransportHandlerFileCache.cpp
//
// Created by Nicki Vankoughnett
//    based on UTFInternet/INetFileCache.h
//
// This is strictly a utility class that is a cache of files obtained from 
// the Internet, usually obtained via HTTP and FTP.
///////////////////////////////////////////////////////////////////////////////



#include "BCTransportHandlerFileCache.h"
#include "SharedBuffer.h"
#include "INetMIME.h"
#include "PlatformString.h"
#include <EAIniFile.h>  // EAWebKit has an EAIniFile.h/cpp implementation

#include <EAIO/PathString.h>
#include <EAIO/EAStreamMemory.h>
#include <EAIO/EAFileStream.h>
#include <EAIO/EAFileBase.h>
#include <EAIO/EAFileUtil.h>
#include <EAIO/EAFileDirectory.h>
#include <EAWebKit/internal/EAWebKitString.h>
#include <EAWebKit/internal/EAWebKitAssert.h>
#include <EAWebKit/internal/EAWebKitEASTLHelpers.h>
#include <stdio.h>
#include <time.h>


namespace EA
{

namespace WebKit
{

int32_t GetHTTPTime()   //as implemented in HTTPBase.cpp from UTFInternet package
{
    return (int32_t)time(NULL);
}



// Constants
//

const uint32_t   kDefaultAccessCountBeforeMaintenance  = 40;                 // Number of file accesses before we purge values.
const uint32_t   kMaxPracticalFileSize                 = 1000000 * 4;        // ~4 MB TODO: this is questionable, probably should be settable option
const uint32_t   kDefaultFileCacheSize                 = 1000000 * 3;        // ~3 MB
const uint32_t   kDefaultExpirationTimeSeconds         = 60 * 60 * 5;        // 5 hours
const bool       kDefaultKeepExpired                   = false;
const char16_t*  kDefaultCacheDirectoryName            = L"EA Network Cache\\";
const char16_t*  kDefaultIniFileName                   = L"FileCache.ini";
const char16_t*  kDefaultIniFileSection                = L"Cache Entries";
const char16_t*  kCachedFileExtension                  = L".cache";
const char16_t*  kSearchCachedFileExtension            = L"*.cache";          // This should work on all platforms



// THREAD_SAFE_CALL / THREAD_SAFE_INNER_CALL
// 
// If thread safety is enabled (and it usually is), these defines map 
// to mutex locks and verification of mutex locks respectively.
//
#if IFC_THREAD_SAFE
#define THREAD_SAFE_CALL        EA::Thread::AutoMutex autoMutex(mMutex)
#define THREAD_SAFE_INNER_CALL  EAW_ASSERT(mMutex.HasLock())
#else
#define THREAD_SAFE_CALL
#define THREAD_SAFE_INNER_CALL
#endif


/******************Implementation of CacheResponseHeaderInfo******************/

/*
From:  http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html

Cache-Control   = "Cache-Control" ":" 1#cache-directive
cache-directive = cache-request-directive
     | cache-response-directive
cache-request-directive =
       "no-cache"                          ; Section 14.9.1
     | "no-store"                          ; Section 14.9.2
     | "max-age" "=" delta-seconds         ; Section 14.9.3, 14.9.4
     | "max-stale" [ "=" delta-seconds ]   ; Section 14.9.3
     | "min-fresh" "=" delta-seconds       ; Section 14.9.3
     | "no-transform"                      ; Section 14.9.5
     | "only-if-cached"                    ; Section 14.9.4
     | cache-extension                     ; Section 14.9.6
 cache-response-directive =
       "public"                               ; Section 14.9.1
     | "private" [ "=" <"> 1#field-name <"> ] ; Section 14.9.1
     | "no-cache" [ "=" <"> 1#field-name <"> ]; Section 14.9.1
     | "no-store"                             ; Section 14.9.2
     | "no-transform"                         ; Section 14.9.5
     | "must-revalidate"                      ; Section 14.9.4
     | "proxy-revalidate"                     ; Section 14.9.4
     | "max-age" "=" delta-seconds            ; Section 14.9.3
     | "s-maxage" "=" delta-seconds           ; Section 14.9.3
     | cache-extension                        ; Section 14.9.6
cache-extension = token [ "=" ( token | quoted-string ) ]


*/

CacheResponseHeaderInfo::CacheResponseHeaderInfo() : m_ShouldCacheToDisk(m_ShouldCacheToDisk), m_MaxAge(0)
{   

}

void CacheResponseHeaderInfo::Reset()
{
    m_ShouldCacheToDisk = false;
    m_MaxAge = 0;
    m_Header.clear();
}


void CacheResponseHeaderInfo::ApplyHeaderDirective(const EA::WebKit::FixedString16& directive)
{
    EA::WebKit::FixedString16::size_type idx = 0;
    if(directive == L"public")
        m_ShouldCacheToDisk = true;
    else if(directive.find(L"private") == 0 || directive == L"no-store")       //  need to account for {private="bleh"}
       m_ShouldCacheToDisk  = false;

    else if( (idx = directive.find(L"max-age")) == 0)
    {
        int32_t result = swscanf( directive.c_str(), L"max-age=%u", &m_MaxAge);
        //make sure we do not get any screwed up results here
        if(result != 1)
            m_MaxAge = 0;

    }
}

void CacheResponseHeaderInfo::SetDirectivesFromHeader(const EA::WebKit::TransportInfo* pTInfo)
{
    Reset();
	
	const HeaderMap& headers = *GET_HEADERMAP(pTInfo->mHeaderMapIn);
	EA::WebKit::HeaderMap::const_iterator entry = headers.find(L"Cache-Control");
    if (entry != headers.end())
    {
		m_Header.assign(entry->second.c_str()); 
        EA::WebKit::FixedString8 uri8, header8;
        EA::WebKit::ConvertToString8(*GET_FIXEDSTRING16(pTInfo->mURI), uri8);
        EA::WebKit::ConvertToString8(m_Header, header8);

        //evaluate header directive substrings within m_Header
        char16_t comma = L',';
        EA::WebKit::FixedString16 tmp;
        uint32_t a=0, b;
        uint32_t len = m_Header.length();

        do
        {
            b = a+1;
            while(b < len && (m_Header[b] != comma))
                ++b;

            ApplyHeaderDirective(m_Header.substr(a,b));

            a = b+1;
        }while(a <= len);
    }
}

/*****************Implementation of TransportHandlerFileCache*****************/
TransportHandlerFileCache::TransportHandlerFileCache()
  : mbInitialized(false)
  , mbEnabled(false)
  , msCacheDirectory ()
  , msIniFileName(kDefaultIniFileName)
  , mDataMap()
  , mbKeepExpired ( kDefaultKeepExpired )
  , mnMaxFileCacheSize ( kDefaultFileCacheSize )
  , mnDefaultExpirationTimeSeconds ( kDefaultExpirationTimeSeconds )
  , mnCacheAccessCount ( 0 ) 
  , mnCacheAccessCountSinceLastMaintenance ( 0 )
{
    // Empty
}

TransportHandlerFileCache::~TransportHandlerFileCache()
{
    Shutdown(NULL);
}

bool TransportHandlerFileCache::UseFileCache(bool enabled) 
{ 
    if(enabled)
    {
        mbEnabled = enabled; 

        //Nicki Vankoughnett:  The return of the Init() function is handled by the base class,
        //and will return true even if it did not properly initialize.
        //so we call init and determine success by checking against initialized.
        Init(NULL);
        mbEnabled = mbInitialized;
    }
    else
    {
        Shutdown(NULL);
    }
    return mbEnabled;
}

bool TransportHandlerFileCache::Init(const char16_t* pScheme)
{
    THREAD_SAFE_CALL;

    bool bReturnValue = true;

    if(!mbInitialized && mbEnabled)
    {
        // To do: We need to support the case whereby we are disabled and don't use a cache directory.
 
        if(!msCacheDirectory.length())
        {
            char16_t pDirectory[EA::IO::kMaxPathLength];
            EA::IO::GetTempDirectory(pDirectory);
            msCacheDirectory = pDirectory;
            msCacheDirectory += kDefaultCacheDirectoryName;
        }

        if(!EA::IO::Directory::Exists(msCacheDirectory.c_str()))
        {
            bReturnValue = EA::IO::Directory::Create(msCacheDirectory.c_str());
            EAW_ASSERT_MSG(bReturnValue, "TransportHandlerFileCache::Init(): Unable to create cache directory.");
        }

        ReadCacheIniFile();
        RemoveUnusedCachedFiles();

        mbInitialized = true;
    }

    return bReturnValue;
}

bool TransportHandlerFileCache::Shutdown(const char16_t* /*pScheme*/)
{
    THREAD_SAFE_CALL;

    if(mbInitialized)
    {
        DoPeriodicCacheMaintenance();
        UpdateCacheIniFile();
        ClearCacheMap();
        mbInitialized = false;
    }
    mbEnabled = false;
    return true;
}

bool TransportHandlerFileCache::InitJob(TransportInfo* pTInfo, bool& bStateComplete)
{
    using namespace EA::WebKit;

    // We return true if we feel we can handle the job.
    FileSystem* pFS = GetFileSystem();

    if(pFS != NULL)
    {
        Allocator* pAllocator = GetAllocator();
        FileInfo*  pFileInfo  = new(pAllocator->Malloc(sizeof(FileInfo), 0, "EAWebKit/TransportHandlerFileCache")) FileInfo; 

        pTInfo->mTransportHandlerData = (uintptr_t)pFileInfo;
        bStateComplete = true;

        //Obtain the name of the temp file that we cached this file under
        EA::WebKit::FixedString8 uri8;
        EA::WebKit::ConvertToString8(*GET_FIXEDSTRING16(pTInfo->mURI), uri8);
        DataMap::iterator iter = mDataMap.find( uri8 );
        if (iter != mDataMap.end() )
        {
            const Info& cacheFileInfo = iter->second;

            EA::IO::Path::PathString16 sFilePath(msCacheDirectory.c_str());
            EA::IO::Path::Join(sFilePath, cacheFileInfo.msCachedFileName.c_str() );
            EA::IO::FileStream fileStream(sFilePath.c_str());
            
            EA::WebKit::ConvertToString8(sFilePath.c_str(), *GET_FIXEDSTRING8(pTInfo->mPath));

        }


        #ifdef EA_DEBUG
            mJobCount++;
        #endif

        return true;
    }

    return false;
}

bool TransportHandlerFileCache::ShutdownJob(TransportInfo* pTInfo, bool& bStateComplete)
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

bool TransportHandlerFileCache::Connect(TransportInfo* pTInfo, bool& bStateComplete)
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
            if(pFS->OpenFile(pFileInfo->mFileObject, GET_FIXEDSTRING8(pTInfo->mPath)->c_str(), FileSystem::kRead))
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

bool TransportHandlerFileCache::Disconnect(TransportInfo* pTInfo, bool& bStateComplete)
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

bool TransportHandlerFileCache::Transfer(TransportInfo* pTInfo, bool& bStateComplete)
{
    using namespace EA::WebKit;

    bool bResult = true;

    FileInfo* pFileInfo = (FileInfo*)pTInfo->mTransportHandlerData;
    EAW_ASSERT(pFileInfo != NULL);

    if(pFileInfo->mFileObject != FileSystem::kFileObjectInvalid)
    {
        FileSystem* pFS = GetFileSystem();
        EAW_ASSERT(pFS != NULL);

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

void TransportHandlerFileCache::InvalidateCachedData(const EA::WebKit::TransportInfo* pTInfo)
{
    if(!mbEnabled)
        return;

    //Some HTTP methods MUST cause a cache to invalidate an entity. This is either 
    //the entity referred to by the Request-URI, or by the Location or 
    //Content-Location headers (if present). These methods are: POST, PUT, DELETE.

    bool invalidate =   EA::Internal::Stricmp("POST",   pTInfo->mMethod ) == 0 || 
                        EA::Internal::Stricmp("PUT",    pTInfo->mMethod ) == 0 || 
                        EA::Internal::Stricmp("DELETE", pTInfo->mMethod ) == 0;

    if(invalidate)
    {

        EA::WebKit::FixedString8 uri8;
		EA::WebKit::ConvertToString8(*GET_FIXEDSTRING16(pTInfo->mURI), uri8);
        DataMap::iterator iter = mDataMap.find( uri8 );
        if (iter != mDataMap.end() )
        {
            const Info& cacheFileInfo = iter->second;
            RemoveCachedFile(cacheFileInfo.msCachedFileName.c_str());
            mDataMap.erase(iter);
        }
    }
}

void TransportHandlerFileCache::CacheToDisk(const EA::WebKit::FixedString16& uriFNameStr, const EA::WebKit::FixedString8& mimeStr, const WebCore::SharedBuffer& requestData)
{
    THREAD_SAFE_CALL;
    // Writes the data in the requestData object to a cache file, and enters the relevant info into the 
    //mDataMap object.

    if(!mbEnabled)
        return;


    bool bSuccess = false;

    EA::WebKit::FixedString8 pKey;
    EA::WebKit::ConvertToString8(uriFNameStr, pKey);

    RemoveCachedData ( pKey ); // if we have something for this key already, purge it

    Info& newInfo ( (*mDataMap.insert ( eastl::make_pair (  pKey, Info() ) ).first).second );
    newInfo.mnDataSize     = 0;
    newInfo.mnLocation     = 0;
    newInfo.mnTimeoutSeconds = 0;
    newInfo.mnTimeCreated  = GetHTTPTime ();
    newInfo.mnTimeLastUsed = UINT32_MAX;    // trick so FindLRU will never find un-committed items
    newInfo.mnTimeTimeout  = newInfo.mnTimeCreated;
    newInfo.msCachedFileName = uriFNameStr.c_str();
    newInfo.msMIMEContentType = mimeStr;

    bool validName = false;
    
	
	EA::WebKit::FixedString16 tmp(msCacheDirectory);
	MIMEType mimeType, mimeSubtype;
    if ( MIMEStringToMIMETypes( mimeStr.c_str(), mimeType, mimeSubtype, (uint32_t) mimeStr.size()) )
    {
        newInfo.msCachedFileName = uriFNameStr.c_str();
        validName = GetNewCacheFileName ( mimeType, mimeSubtype, newInfo.msCachedFileName);
		
	tmp +=newInfo.msCachedFileName;
    }

	EA::WebKit::FixedString8 pathStr;
	EA::WebKit::ConvertToString8( tmp, pathStr );

    // We return true if we feel we can handle the job.
    FileSystem* pFS = GetFileSystem();
    if(pFS != NULL && validName)
    {
        FileInfo fileInfo;

        fileInfo.mFileObject = pFS->CreateFileObject();
        if(fileInfo.mFileObject != FileSystem::kFileObjectInvalid)
        {
            if(pFS->OpenFile(fileInfo.mFileObject, pathStr.c_str(), FileSystem::kWrite))
            {
                bSuccess = pFS->WriteFile(fileInfo.mFileObject, requestData.data(), requestData.size());
                pFS->CloseFile(fileInfo.mFileObject);

                if(bSuccess)
                {
                    newInfo.mnLocation = kCacheLocationDisk;
                    newInfo.mnDataSize = requestData.size();

                    newInfo.mnTimeoutSeconds = kDefaultExpirationTimeSeconds;
                    newInfo.mnTimeTimeout = newInfo.mnTimeCreated + newInfo.mnTimeoutSeconds;
                    newInfo.mnTimeLastUsed  = GetHTTPTime ();
                }
            }
            pFS->DestroyFileObject(fileInfo.mFileObject);
        }
    }

    if ( !bSuccess )  // failed to write cache file, so erase the cache entry
        mDataMap.erase( pKey );

    if ( ++mnCacheAccessCountSinceLastMaintenance > kDefaultAccessCountBeforeMaintenance )
        DoPeriodicCacheMaintenance();
}

bool TransportHandlerFileCache::IniFileCallbackFunction(const char16_t* /*pKey*/, const char16_t* pValue, void* pContext)
{


    // Thread-safety not implemented here, as this is an internal function.
    const uint32_t   kIniFileFieldCount   ( 8 );

    if(*pValue)
    {
        TransportHandlerFileCache* const pFileCacheHandler = reinterpret_cast<TransportHandlerFileCache*>(pContext);
        if(!pFileCacheHandler->mbEnabled)
            return false;

        EA::WebKit::FixedString16 sTemp(pValue);
        EA::WebKit::FixedString16 sFields[ kIniFileFieldCount ];
        eastl_size_t  i, nCurrentFieldIndex ( kIniFileFieldCount - 1 );

        // The ini file value is a comma-delimited set of strings. Note that we intentionally 
        // don't use sscanf to parse this, as it wouldn't always work, due to the kinds of 
        // strings that could be subcomponents.
        for(i = sTemp.length() - 1; ((int)(unsigned)i >= 0) && (nCurrentFieldIndex > 0); i--)
        {
            if(sTemp[i] == ',')
            {
                sFields[nCurrentFieldIndex].assign(sTemp.data() + i + 1, sTemp.length() - (i + 1));  // Assign current field.
                sTemp.erase(i, sTemp.length() - i);                                             // Erase everything from ',' and on.
                nCurrentFieldIndex--;
            }
        }

        EAW_ASSERT(nCurrentFieldIndex == 0); // If the ini file has not been messed with, this should be so.

        if(nCurrentFieldIndex == 0)
        {
            Info cacheInfo;

            sFields[0]                      = sTemp;         // Assign the first field to be the rest of the string.
            cacheInfo.msMIMEContentType.sprintf("%ls", sFields[2].c_str());
            cacheInfo.msCachedFileName      = sFields[1];
            cacheInfo.mnDataSize            = EA::Internal::AtoU32(sFields[3].c_str());
            cacheInfo.mnLocation            = kCacheLocationDisk;
            cacheInfo.mnTimeoutSeconds      = EA::Internal::AtoU32(sFields[4].c_str());
            cacheInfo.mnTimeCreated         = EA::Internal::AtoU32(sFields[5].c_str());
            cacheInfo.mnTimeLastUsed        = EA::Internal::AtoU32(sFields[6].c_str());
            cacheInfo.mnTimeTimeout         = EA::Internal::AtoU32(sFields[7].c_str());

            if(cacheInfo.mnDataSize <= kMaxPracticalFileSize)
            {
                const uint32_t nTimeNow = (uint32_t)GetHTTPTime();

                if(cacheInfo.mnTimeCreated > nTimeNow)  // Just fix the error.
                    cacheInfo.mnTimeCreated = (uint32_t)nTimeNow;

                if(cacheInfo.mnTimeLastUsed > nTimeNow) // Just fix the error
                    cacheInfo.mnTimeLastUsed = (uint32_t)nTimeNow;

                if(cacheInfo.mnTimeTimeout > nTimeNow)  // If file has not expired...
                {
                    EA::IO::Path::PathString16 sFilePath(pFileCacheHandler->msCacheDirectory.c_str());
                    EA::IO::Path::Join(sFilePath, cacheInfo.msCachedFileName.c_str());

                    if(EA::IO::File::Exists(sFilePath.c_str()))
                    {
                        EA::WebKit::FixedString8 sField8;
                        EA::WebKit::ConvertToString8(sFields[0], sField8);
                        pFileCacheHandler->mDataMap.insert(DataMap::value_type(sField8, cacheInfo));
                    }
                }
                else
                {
                    pFileCacheHandler->RemoveCachedFile(cacheInfo.msCachedFileName.c_str());
                }
            }
            else
            {
                EAW_ASSERT_MSG(cacheInfo.mnDataSize > kMaxPracticalFileSize, "TransportHandlerFileCache::IniFileCallbackFunction: File appears to be impossibly large.\n");
                pFileCacheHandler->RemoveCachedFile(cacheInfo.msCachedFileName.c_str());
            }
        }
        else
        {
            EAW_ASSERT_MSG(nCurrentFieldIndex != 0, "TransportHandlerFileCache::IniFileCallbackFunction: Corrupt ini file entry.\n");
        }
    }

    return true;
}

void TransportHandlerFileCache::SetDefaultExpirationTime(uint32_t nDefaultExpirationTimeSeconds)
{
    // We don't worry about thread safety here, as this function is only to be 
    // called from a single thread upon init.

    mnDefaultExpirationTimeSeconds = nDefaultExpirationTimeSeconds;
}

const EA::WebKit::FixedString16& TransportHandlerFileCache::GetCacheDirectory()
{
    /// The returned directory will end with a trailing path separator. 
 
    // We don't worry about thread safety here, as this value is assumed
    // to be set once upon init and not changed.

    return msCacheDirectory;
}

bool TransportHandlerFileCache::SetCacheDirectory(const char16_t* pCacheDirectory)
{
    // The supplied directory must end with a trailing path separator.
    // The supplied directory string must be of length <= the maximum
    // designated path length for the given platform; otherwise this 
    // function will fail.
    // If the directory could not be created or accessed, this function
    // will fail.

    // We don't worry about thread safety here, as this function is only to be 
    // called from a single thread upon init.

    bool bReturnValue = false;

    if(pCacheDirectory[0])
    {
        // If we are going to change the cache directory, we may as well not leave
        // any cache files behind in the old cache directory.
        ClearCache();

        msCacheDirectory = pCacheDirectory;

        msCacheDirectory.push_back(0);
        if(!EA::IO::Path::EnsureTrailingSeparator(&msCacheDirectory[0], EA::IO::kLengthNull))
            msCacheDirectory.pop_back();

        if(EA::IO::Directory::Exists(msCacheDirectory.c_str()))
            bReturnValue = true;
        else
        {
            bReturnValue = EA::IO::Directory::Create(msCacheDirectory.c_str());
            EAW_ASSERT_MSG(bReturnValue, "TransportHandlerFileCache::SetCacheDirectory(): Unable to create cache directory.");
        }
    }

    return bReturnValue;
}

bool TransportHandlerFileCache::SetCacheDirectory(const char8_t* pCacheDirectory)
{
    //convenience wrapper.  We will convert the string into a 16 bit string, 
    //then call the above function.
	//Note by Arpit Baldeva: Old code did not do UTF encoding conversions. Replace it with this.
	EA::WebKit::FixedString8 cacheDir8(pCacheDirectory);
	EA::WebKit::FixedString16 cacheDir16;

	EA::WebKit::ConvertToString16(cacheDir8, cacheDir16);
	return SetCacheDirectory(cacheDir16.c_str());
/*
    int pathLen = EA::Internal::Strlen(pCacheDirectory);
    EA::WebKit::FixedString16 dir16;
    dir16.reserve(pathLen);
    dir16.resize(pathLen);
    int writeCount = EA::Internal::Strlcpy(&dir16[0], pCacheDirectory, dir16.max_size(), pathLen); (void)writeCount;
    EAW_ASSERT(writeCount == pathLen);
    return SetCacheDirectory(dir16.c_str());
	*/
}

void TransportHandlerFileCache::GetCacheDirectory(EA::WebKit::FixedString16& cacheDirectory)
{
    cacheDirectory = msCacheDirectory;
}

void TransportHandlerFileCache::GetCacheDirectory(EA::WebKit::FixedString8& cacheDirectory)
{
    EA::WebKit::FixedString16::size_type pathLen = msCacheDirectory.length();
    cacheDirectory.reserve(pathLen);
    cacheDirectory.resize(pathLen);
    int writeCount = EA::Internal::Strlcpy(&cacheDirectory[0], msCacheDirectory.c_str(), cacheDirectory.max_size(), pathLen); (void)writeCount;
    EAW_ASSERT(writeCount == (int) pathLen);
}

const EA::WebKit::FixedString16& TransportHandlerFileCache::GetCacheIniFileName()
{
    // We don't worry about thread safety here, as this value is assumed
    // to be set once upon init and not changed.

    return msIniFileName;
}

bool TransportHandlerFileCache::SetCacheIniFileName(const char16_t* pCacheIniFileName)
{
    // Sets the file name only. File is always put in same dir as ini file path.
    // The supplied directory string must be of length <= the maximum
    // designated path length for the given platform; otherwise this 
    // function will fail.

    // We don't worry about thread safety here, as this function is only to be 
    // called from a single thread upon init.
    EA::IO::Path::PathString16 path(msCacheDirectory.data(), msCacheDirectory.length());
    EA::IO::Path::Join(path, msIniFileName.c_str());

    if(EA::IO::File::Exists(path.c_str()))
        EA::IO::File::Remove(path.c_str());

    msIniFileName = pCacheIniFileName;
    return true;
}

void TransportHandlerFileCache::SetMaxCacheSize(uint32_t nCacheSize)
{
    // We don't worry about thread safety here, as this function is only to be 
    // called from a single thread upon init.

    uint32_t tmp = mnMaxFileCacheSize;
    
    mnMaxFileCacheSize = nCacheSize;

    if(tmp > mnMaxFileCacheSize)
        DoPeriodicCacheMaintenance();
}


bool TransportHandlerFileCache::GetCachedDataValidity( const EA::WebKit::FixedString16& pURLTxt )
{
    THREAD_SAFE_CALL;

    if(!mbEnabled)
        return false;

    EA::WebKit::FixedString8 tmp;
    EA::WebKit::ConvertToString8(pURLTxt, tmp);
    return GetCachedDataValidity( tmp );
}

bool TransportHandlerFileCache::GetCachedDataValidity( const EA::WebKit::FixedString8& pURLTxt )
{
    THREAD_SAFE_CALL;

    if(!mbEnabled)
        return false;

    DataMap::iterator itSought(mDataMap.find ( pURLTxt ));
    if (itSought != mDataMap.end () ) {
       return GetCachedDataValidity ( (*itSought).second );
    }

    return false;
}

bool TransportHandlerFileCache::GetCachedDataValidity( const Info& cacheInfo )
{
    if(!mbEnabled)
        return false;

    if ( (cacheInfo.mnLocation & kCacheLocationPending) == 0 ) {
        return cacheInfo.mnTimeTimeout > (uint32_t) GetHTTPTime (); // Question: Why in the world would time ever be negative?
    }

    return false;
}

bool TransportHandlerFileCache::GetCachedDataInfo(const EA::WebKit::FixedString8& pKey, Info& fileCacheInfo) const
{
    // Low level accessor. Gets copy of the data. Gets copy of data. Does *not* do expiration checks,
    // and so may return information for data that has expired. This is by design, as this function's
    // purpose is to allow the interpretation of the cached data as it currently is.
    THREAD_SAFE_CALL;

    if(!mbEnabled)
        return false;

    DataMap::const_iterator it = eastl::hashtable_find(mDataMap, pKey.c_str()); 

    if(it != mDataMap.end())
    {
        const Info& infci = (*it).second;
        fileCacheInfo = infci;
        return true;
    }
    return false;
}


void TransportHandlerFileCache::ClearCache()
{
    THREAD_SAFE_CALL;

    mnCacheAccessCount++;

    DataMap::iterator iter;
    for(iter = mDataMap.begin(); iter != mDataMap.end(); ++iter)
    {
        Info& cacheInfo = (*iter).second;
        RemoveCachedFile(cacheInfo.msCachedFileName.c_str());
    }
    mDataMap.clear();
    UpdateCacheIniFile();
}


bool TransportHandlerFileCache::RemoveCachedData(const EA::WebKit::FixedString8& pKey)
{
    // Consider: make an internal version of this that works on Info
    THREAD_SAFE_CALL;

    mnCacheAccessCount++;

    // To consider: Make this periodic maintenance based on time rather than access count.
    if ( ++mnCacheAccessCountSinceLastMaintenance > kDefaultAccessCountBeforeMaintenance ) {
        DoPeriodicCacheMaintenance();
    }

    DataMap::iterator it = eastl::hashtable_find(mDataMap, pKey.c_str());

    if(it != mDataMap.end()) // If the key is already in our data map...
    {
        Info& cacheInfo = (*it).second;
        RemoveCachedFile(cacheInfo.msCachedFileName.c_str());   // Remove the file if it is present.
        mDataMap.erase(it);                                     // Just erase it and move on.

        return true;
    }

    return false;
}

bool TransportHandlerFileCache::GetNewCacheFileName( int nMIMEType, int nMIMESubtype, EA::WebKit::FixedString16& sFileName)
{
    if(!mbEnabled)
        return false;

    // Gets a new cached file name to use. The returned name is a file name only 
    // and the directory is the cache directory (See GetCacheDirectory/SetCacheDirectory).
    // GetNewCacheFilePath returns the same value but with the directory prepended.
    // 
    // If the input pKey looks like a file name (defined by having a dot in it), 
    // this function attempts to retain that extension in the returned file name. 

    // No thread safety checks for this method. It doesn't touch class data.

    char16_t pExtension[EA::IO::kMaxPathLength] = { '\0' };

    MIMETypesToFileExtension ( MIMEType(nMIMEType), MIMEType(nMIMESubtype), pExtension, EA::IO::kMaxPathLength );

    EA::Internal::Strcat ( pExtension, kCachedFileExtension );

    char16_t pFilePath[EA::IO::kMaxPathLength];

    //Nicki Vankoughnett:  It is very strange that this function results in the creation of an empty
    //file.  It does not cause any problems right now, but it is not ideal given that it will
    //result in a [open->close->open->write->close] chain of events.  That may be a TRC issue
    //on consoles with respect to disk access.
    const bool bResult = EA::IO::MakeTempPathName(pFilePath, msCacheDirectory.c_str(), NULL, pExtension);

    if ( bResult ) {
        // we know that the full path begins with the directory, so just copy the string after that
        sFileName.assign ( &pFilePath [ msCacheDirectory.size () ] );
    }

    return bResult;
}

void TransportHandlerFileCache::ClearCacheMap()
{
    mDataMap.clear();
}



// UpdateCacheIniFile
//
// This function is normally called upon shutting down an instance of this class.
//   CA: started calling this from DoPeriodicCacheMaintenance to avoid ini file being out of
//   date on non-clean exit
//
bool TransportHandlerFileCache::UpdateCacheIniFile()
{
    // Writes our cache information to the ini file used to store it.
    THREAD_SAFE_INNER_CALL;

    if(!mbEnabled)
        return false;

    EA::IO::Path::PathString16 sIniFilePath(msCacheDirectory.c_str());
    EA::IO::Path::Join(sIniFilePath, msIniFileName.c_str());

    // Just remove the old ini file outright. We won't be needing it any more.
    EA::IO::File::Remove(sIniFilePath.c_str());

    EA::WebKit::IniFile iniFile(sIniFilePath.c_str());
    int i = 0;

    for(DataMap::iterator it = mDataMap.begin(); it != mDataMap.end(); ++it) // For each hash map entry, add an ini file section for it.
    {
        const EA::WebKit::FixedString8& sKey = (*it).first;
        Info& cacheInfo = (*it).second;

        if( ( cacheInfo.mnLocation & kCacheLocationDisk ) != 0 ) // only write about things on disk
        {
            char16_t pIniFileKey[64];
            char16_t pIniFileValue[2048];

            swprintf(pIniFileKey,64, L"CacheEntry%05d", i);
#ifdef _MSC_VER
			#define kFormat L"%hs,%ls,%hs,%u,%u,%u,%u,%u" // Specific case for Microsoft platforms; it treats %s as %ls and expects you to use %hs to specify char8_t instead of char16_t.
#else
			#define kFormat L"%s,%ls,%s,%u,%u,%u,%u,%u"
#endif
			swprintf(pIniFileValue, 2048, kFormat, sKey.c_str(), cacheInfo.msCachedFileName.c_str(), cacheInfo.msMIMEContentType.c_str(), cacheInfo.mnDataSize, cacheInfo.mnTimeoutSeconds, cacheInfo.mnTimeCreated, cacheInfo.mnTimeLastUsed, cacheInfo.mnTimeTimeout);

            iniFile.WriteEntry(kDefaultIniFileSection, pIniFileKey, pIniFileValue);

            ++i;
        }
    }

    return true;
}

bool TransportHandlerFileCache::ReadCacheIniFile()
{
    if(!mbEnabled)
        return false;

    // Thread-safety not implemented here, as this is only called from init.
    EA::IO::Path::PathString16 sIniFilePath(msCacheDirectory.c_str());
    EA::IO::Path::Join(sIniFilePath, msIniFileName.c_str());

    if (EA::IO::File::Exists(sIniFilePath.c_str())) { // inifile doesn't handle if it doesn't exist
        EA::WebKit::IniFile iniFile(sIniFilePath.c_str());
        iniFile.EnumEntries(kDefaultIniFileSection, IniFileCallbackFunction, this);
    }

    return true;
}

bool TransportHandlerFileCache::RemoveCachedFile(const char16_t* pFileName)
{
    // This is a function which accepts a file name (name only, not including directory)
    // and returns the size of it. The argument takes a EA::WebKit::FixedString16& instead of char16_t* because
    // the only way it will ever be called is via a EA::WebKit::FixedString16.
    // Thread-safety not implemented here. Sometimes it's called from thread safe code and sometimes not

    if(pFileName && *pFileName)
    {
        EA::IO::Path::PathString16 sFilePath(msCacheDirectory.c_str());
        EA::IO::Path::Join(sFilePath, pFileName);

        return EA::IO::File::Remove(sFilePath.c_str());
    }

    return false;
}

bool TransportHandlerFileCache::RemoveUnusedCachedFiles()
{
    // Thread-safety not implemented here, as this is only called from init.

    EA::IO::DirectoryIterator            directoryIterator;
    EA::IO::DirectoryIterator::EntryList entryList;

    if(directoryIterator.Read(msCacheDirectory.c_str(), entryList, kSearchCachedFileExtension, EA::IO::kDirectoryEntryFile))
    {
        for(EA::IO::DirectoryIterator::EntryList::iterator it = entryList.begin(); it != entryList.end(); ++it)
        {
            const EA::IO::DirectoryIterator::Entry& entry = *it;
            DataMap::iterator itMap = mDataMap.begin();

            for(; itMap != mDataMap.end(); ++itMap) // For each hash map entry, add an ini file section for it.
            {
                const Info& cacheInfo = (*itMap).second;

                if(EA::Internal::Stricmp(cacheInfo.msCachedFileName.c_str(), entry.msName.c_str()) == 0) // If the disk file is also in our cache map...
                    break;
            }

            if(itMap == mDataMap.end()) // If the file was not in our list...
                RemoveCachedFile(entry.msName.c_str());
        }
    }

    return true;
}

TransportHandlerFileCache::DataMap::iterator TransportHandlerFileCache::FindLRUItem()
{
//   find oldest item in location
    THREAD_SAFE_INNER_CALL;

    DataMap::iterator itCur(mDataMap.begin()), itEnd(mDataMap.end());
    DataMap::iterator itSought ( itEnd );
    uint32_t nOldestTime ( UINT32_MAX ); // Note: pending resources have last access == UINT32_MAX so they should never be chosen
    for(; itCur!=itEnd; ++itCur) 
    {
    	Info& cacheInfo ( (*itCur).second );
        if ( cacheInfo.mnTimeLastUsed < nOldestTime ) 
        {
            itSought = itCur;
            nOldestTime = cacheInfo.mnTimeLastUsed;
        }
    }

    return itSought;
}

void TransportHandlerFileCache::DoPeriodicCacheMaintenance()
{
    THREAD_SAFE_INNER_CALL;

    if(!mbEnabled)
        return;

    //EA::IO::size_type nRAMCacheMemoryUsage  = 0;
    EA::IO::size_type nFileCacheMemoryUsage = 0;
    
    bool    bDiskFilesChanged = false; // should we update the ini file?

    mnCacheAccessCountSinceLastMaintenance = 0;

    if ( !mbKeepExpired ) {
        // Need to check expiration dates of data map entries.
        const uint32_t nTimeNow = (uint32_t)GetHTTPTime();

        for(DataMap::iterator it = mDataMap.begin(); it != mDataMap.end(); ) // For each hash map entry, add an ini file section for it.
        {
            const Info& cacheInfo = (*it).second;

            if ( (nTimeNow >= cacheInfo.mnTimeTimeout) 
                && ( ( cacheInfo.mnLocation & kCacheLocationPending ) == 0 ) ) 
                // If the cached item has expired...
                // && don't delete files that have not been committed
            {
                if ( cacheInfo.mnLocation & kCacheLocationDisk ) {
                    RemoveCachedFile(cacheInfo.msCachedFileName.c_str());   // Delete File cached data if present.
                    bDiskFilesChanged = true;
                }
                it = mDataMap.erase(it);
            }
            else
            {
                if ( cacheInfo.mnLocation & kCacheLocationDisk ) {
                    nFileCacheMemoryUsage += cacheInfo.mnDataSize;
                }
                ++it;
            }
        }
    }

    // OK, now we've purged any old files and know how much File 
    // space is being taken up by the cached data. We should do some purges
    // of the first-expiring data if we are using up too much memory.

    while ( nFileCacheMemoryUsage > mnMaxFileCacheSize ) 
    {
        DataMap::iterator itLRU ( FindLRUItem() );
        if ( itLRU != mDataMap.end () ) 
        {
            Info& cacheInfo ( (*itLRU).second );
            RemoveCachedFile ( cacheInfo.msCachedFileName.c_str() );
            bDiskFilesChanged = true;
            nFileCacheMemoryUsage -= cacheInfo.mnDataSize;
            mDataMap.erase ( itLRU );
        }
        else 
        {
            break; // shouldn't happen
        }
    }

    if ( bDiskFilesChanged )
    	UpdateCacheIniFile();
}

} //namespace WebKit
} //namespace EA
