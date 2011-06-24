/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

/*
* This file was modified by Electronic Arts Inc Copyright � 2009
*/

#ifndef ApplicationCacheStorage_h
#define ApplicationCacheStorage_h

#include <wtf/FastAllocBase.h>
#if ENABLE(OFFLINE_WEB_APPLICATIONS)

#include "PlatformString.h"
#include "SQLiteDatabase.h"
#include "StringHash.h"

#include <wtf/HashCountedSet.h>

namespace WebCore {

class ApplicationCache;
class ApplicationCacheGroup;
class ApplicationCacheResource;
class KURL;
    
class ApplicationCacheStorage: public WTF::FastAllocBase {
public:
    void setCacheDirectory(const String&);
    
    ApplicationCacheGroup* cacheGroupForURL(const KURL&);

    ApplicationCacheGroup* findOrCreateCacheGroup(const KURL& manifestURL);
    void cacheGroupDestroyed(ApplicationCacheGroup*);
        
    void storeNewestCache(ApplicationCacheGroup*);
    void store(ApplicationCacheResource*, ApplicationCache*);

    void remove(ApplicationCache*);
    
    void empty();
private:
    PassRefPtr<ApplicationCache> loadCache(unsigned storageID);
    ApplicationCacheGroup* loadCacheGroup(const KURL& manifestURL);
    
    bool store(ApplicationCacheGroup*);
    bool store(ApplicationCache*);
    bool store(ApplicationCacheResource*, unsigned cacheStorageID);

    void loadManifestHostHashes();
    
    void verifySchemaVersion();
    
    void openDatabase(bool createIfDoesNotExist);
    
    bool executeStatement(SQLiteStatement&);
    bool executeSQLCommand(const String&);
    
    String m_cacheDirectory;

    SQLiteDatabase m_database;
    
    // In order to quickly determinate if a given resource exists in an application cache,
    // we keep a hash set of the hosts of the manifest URLs of all cache groups.
    HashCountedSet<unsigned, AlreadyHashed> m_cacheHostSet;
    
    typedef HashMap<String, ApplicationCacheGroup*> CacheGroupMap;
    CacheGroupMap m_cachesInMemory;
};
 
ApplicationCacheStorage& cacheStorage();
    
} // namespace WebCore

#endif // ENABLE(OFFLINE_WEB_APPLICATIONS)

#endif // ApplicationCacheStorage_h
