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
// BCTransportHandlerFileCache.h
//
// Created by Nicki Vankoughnett
//    based on UTFInternet/INetFileCache.h
//
// This is strictly a utility class that is a cache of files obtained from 
// the Internet, usually obtained via HTTP and FTP.
///////////////////////////////////////////////////////////////////////////////



#ifndef TRANSPORT_HANDLER_FILE_CACHE_HEADER
#define TRANSPORT_HANDLER_FILE_CACHE_HEADER

#include "config.h"
#include <wtf/FastAllocBase.h>

#include <EAWebKit/EAWebKit.h>
#include <EAWebKit/EAWebKitTransport.h>
#include <EAWebKit/internal/EAWebKitEASTLHelpers.h>
#include <EASTL/core_allocator_adapter.h>
#include <EASTL/string.h>
#include <EASTL/hash_map.h>
#include <EAIO/EAStream.h>
#include <EAIO/EAStreamMemory.h>


namespace WebCore
{
    class String;
    class SharedBuffer;
}


namespace EA
{
    namespace WebKit
    {

        /// CacheResponseHeaderInfo
        ///
        /// Extracts useful info from the header directives  The Cache handler class will
        /// Use that info to decide if it ought to load from the cache, or save to it.
        ///
        class CacheResponseHeaderInfo: public WTF::FastAllocBase
        {
        public:
            CacheResponseHeaderInfo();
            void SetDirectivesFromHeader(const TransportInfo* pTInfo);

        public:
			FixedString16 m_Header;
            bool          m_ShouldCacheToDisk;
            uint32_t      m_MaxAge;            //The origin server specifies a freshness lifetime for the entity, overriding lifetime values determined by the proxy caches.

        private:
            void Reset();
            void ApplyHeaderDirective(const FixedString16& directive);
        };


        /// TransportHandlerFileCache (formerly INetFileCache)
        ///
        /// Caches files that are presumably (but not necessarily) obtained via the 
        /// Internet via HTTP or possibly FTP.
        ///
        /// This class works by storing key/value pairs. The key is a user-defined string
        /// which can be anything that the user needs it to be. The value is data which 
        /// the user wants cached. This class stores the value as a file on disk or in
        /// memory. When this class is shut down, all data stored in memory is copied to 
        /// disk for persistence while the application is not running. An important aspect
        /// of this file cache is the concept of expiration times. Normally, data that 
        /// has expired is removed and no longer available to the user. There is an option
        /// to keep files beyond their expiration dates as some HTTP caching scheme can
        /// work with cached data beyond their expiration date.
        ///
        /// If IFC_THREAD_SAFE is defined to 1, then this class is internally thread-safe; 
        /// it implements a mutex to protect access via multiple threads. Otherwise thread
        /// safety is left up to the user.
        ///

        class TransportHandlerFileCache : public TransportHandler
        {
        public:
            TransportHandlerFileCache();
           ~TransportHandlerFileCache();

            bool Init       (const char16_t* pScheme);
            bool Shutdown   (const char16_t* pScheme);
            bool InitJob    (TransportInfo* pTInfo, bool& bStateComplete);
            bool ShutdownJob(TransportInfo* pTInfo, bool& bStateComplete);
            bool Connect    (TransportInfo* pTInfo, bool& bStateComplete);
            bool Disconnect (TransportInfo* pTInfo, bool& bStateComplete);
            bool Transfer   (TransportInfo* pTInfo, bool& bStateComplete);


            // Some caching schemes involve keeping resources past their expiration dates, then asking the
            // server if they can still be used. If this is enabled, resources are only deleted when the
            // total size exceeds the limits set.
            void SetKeepExpired(bool bEnable)   {  mbKeepExpired = bEnable;  }
            bool GetKeepExpired()               {  return mbKeepExpired;  }

            //Evaluate and apply cache header
            void InvalidateCachedData(const TransportInfo* pTInfo);
            void CacheToDisk(const FixedString16& uriFNameStr, const FixedString8& mimeStr, const WebCore::SharedBuffer& requestData);

            //Other functionality
            bool CacheEnabled() { return mbEnabled; }
            bool UseFileCache(bool enabled);

            void ClearCache(); //clears the disk cache on demand.

            void  SetDefaultExpirationTime(uint32_t nDefaultExpirationTimeSeconds);
            const FixedString16& GetCacheDirectory();
            bool  SetCacheDirectory(const char16_t* pCacheDirectory);
            bool  SetCacheDirectory(const char8_t* pCacheDirectory);

            void  GetCacheDirectory(EA::WebKit::FixedString16& cacheDirectory);
            void  GetCacheDirectory(EA::WebKit::FixedString8& cacheDirectory);


            const FixedString16& GetCacheIniFileName();
            bool  SetCacheIniFileName(const char16_t* pCacheIniFileName);
            void  SetMaxCacheSize(uint32_t nCacheSize);
            uint32_t GetMaxCacheSize() {  return mnMaxFileCacheSize;  }

            bool GetCachedDataValidity( const FixedString16& url );
            bool GetCachedDataValidity( const FixedString8& url );

        protected:
            // This is a special timeout value that means to never timeout.
            static const uint32_t kTimeoutNever = 0xffffffff;

            enum CacheLocation
            {
                kCacheLocationNone    = 0x0,
                kCacheLocationDisk    = 0x2,
                kCacheLocationPending = 0x4  // an entry for this key has been created, but not committed to the cache
            };

            struct Info
            {
                FixedString8        msMIMEContentType;      /// MIME type (if known) of cached resource
                FixedString16       msCachedFileName;       /// File Name Only. Must append to cache directory to get full path.
                EA::IO::size_type   mnDataSize;             /// Cached file/data size.
                uint32_t            mnLocation;             /// BitField of locations where this is stored. Also indicates validity of cached data (kCacheLocationPending)
                uint32_t            mnTimeoutSeconds;       /// Number of relative seconds before the cache info should timeout. This is used to determine mnTimeTimeout.
                uint32_t            mnTimeCreated;          /// The time this cache info was created.
                uint32_t            mnTimeLastUsed;         /// The time this cache info was last created, updated, or retrieved.
                uint32_t            mnTimeTimeout;          /// The time of the timeout.
            };

            struct FileInfo: public WTF::FastAllocBase
            {
                FileSystem::FileObject mFileObject;
                int64_t                mFileSize;
                char                   mBuffer[4096];

                FileInfo() : mFileObject(FileSystem::kFileObjectInvalid), mFileSize(-1)
                {
                }
            };

            /// cache map data type
            typedef eastl::hash_map<FixedString8, Info, eastl::string_hash<FixedString8>, eastl::equal_to<FixedString8>, EASTLAllocator> DataMap;

            /// This is a callback function which is called by the ini file reader in ReadCacheIniFile.
            static bool IniFileCallbackFunction(const char16_t* pKey, const char16_t* pValue, void* pContext);

            bool GetCachedDataValidity( const Info& cacheInfo );
            bool GetCachedDataInfo(const FixedString8& pKey, Info& fileCacheInfo) const;

            bool RemoveCachedData(const FixedString8& pKey);
            void ClearCacheMap();
            bool UpdateCacheIniFile();
            bool ReadCacheIniFile();

            bool GetNewCacheFileName( int nMIMEType, int nMIMESubtype, FixedString16& sFileName);
            bool RemoveCachedFile(const char16_t* pFileName);
            bool RemoveUnusedCachedFiles();
            void DoPeriodicCacheMaintenance();
            DataMap::iterator FindLRUItem (); // oldest item in location

            // To prevent assignment outside of proper refcounting code, 
            // we declare these functions, but we don't define their implementations.
            TransportHandlerFileCache(const TransportHandlerFileCache&);
            const TransportHandlerFileCache& operator=(const TransportHandlerFileCache&);

        protected:
            bool                        mbInitialized;                          /// Have we been initialized or not?
            bool                        mbEnabled;                              /// Enables or disables the file cache
        #if IFC_THREAD_SAFE
            EA::Thread::AtomicInt32     mRefCount;                              /// thread safe reference count.
            mutable EA::Thread::Mutex   mMutex;                                 /// Standard mutex.
        #else
            int                         mRefCount;                              /// standard refcount
        #endif 
            FixedString16               msCacheDirectory;                       /// Path to cache directory on disk.
            FixedString16               msIniFileName;                          /// File name alone of cache ini file, which stores info about cached data on disk.
            DataMap                     mDataMap;                               /// Map of cached data key (string) to cached data.
            bool                        mbKeepExpired;                          /// Should cached entries be kept after they are expired?
            uint32_t                    mnMaxFileCacheSize;                     /// Max size of file cache.
            uint32_t                    mnDefaultExpirationTimeSeconds;         /// Default time for cache entries to expire.
            uint32_t                    mnCacheAccessCount;                     /// Count of number of times the cache was accessed.
            uint32_t                    mnCacheAccessCountSinceLastMaintenance; /// Count of number of times the cache was accessed since the last time we examined the cache for expirations.

            #ifdef EA_DEBUG
                int mJobCount;  // Used to verify jobs are all shut down and not leaked.
            #endif
        };

    } // namespace WebKit

} // namespace EA



#endif // Header include guard
