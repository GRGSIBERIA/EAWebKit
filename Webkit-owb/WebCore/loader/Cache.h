/*
    This file is part of the KDE libraries

    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/

/*
* This file was modified by Electronic Arts Inc Copyright � 2009
*/

#ifndef Cache_h
#define Cache_h

#include <wtf/FastAllocBase.h>
#include "CachePolicy.h"
#include "CachedResource.h"
#include "PlatformString.h"
#include "StringHash.h"
#include "loader.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

namespace WebCore  {

class CachedCSSStyleSheet;
class CachedResource;
class DocLoader;
class KURL;

// This cache holds subresources used by Web pages: images, scripts, stylesheets, etc.

// The cache keeps a flexible but bounded window of dead resources that grows/shrinks 
// depending on the live resource load. Here's an example of cache growth over time,
// with a min dead resource capacity of 25% and a max dead resource capacity of 50%:

//        |-----|                              Dead: -
//        |----------|                         Live: +
//      --|----------|                         Cache boundary: | (objects outside this mark have been evicted)
//      --|----------++++++++++|
// -------|-----+++++++++++++++|
// -------|-----+++++++++++++++|+++++

class Cache : Noncopyable {
public:
// Placement operator new.
void* operator new(size_t, void* p) { return p; }
void* operator new[](size_t, void* p) { return p; }
 
void* operator new(size_t size)
{
     void* p = fastMalloc(size);
     fastMallocMatchValidateMalloc(p, WTF::Internal::AllocTypeClassNew);
     return p;
}
 
void operator delete(void* p)
{
     fastMallocMatchValidateFree(p, WTF::Internal::AllocTypeClassNew);
     fastFree(p);  // We don't need to check for a null pointer; the compiler does this.
}
 
void* operator new[](size_t size)
{
     void* p = fastMalloc(size);
     fastMallocMatchValidateMalloc(p, WTF::Internal::AllocTypeClassNewArray);
     return p;
}
 
void operator delete[](void* p)
{
     fastMallocMatchValidateFree(p, WTF::Internal::AllocTypeClassNewArray);
     fastFree(p);  // We don't need to check for a null pointer; the compiler does this.
}
private:
    static Cache* staticCache;
public:
    friend Cache* cache();

    // Function to obtain the global cache.
    Cache* cache();
    static void staticInit();
    static void staticFinalize();


    typedef HashMap<String, CachedResource*> CachedResourceMap;

    struct LRUList: public WTF::FastAllocBase {
        CachedResource* m_head;
        CachedResource* m_tail;
        LRUList() : m_head(0), m_tail(0) { }
    };

    struct TypeStatistic: public WTF::FastAllocBase {
        int count;
        int size;
        int liveSize;
        int decodedSize;
        TypeStatistic() : count(0), size(0), liveSize(0), decodedSize(0) { }
    };
    
    struct Statistics: public WTF::FastAllocBase {
        TypeStatistic images;
        TypeStatistic cssStyleSheets;
        TypeStatistic scripts;
#if ENABLE(XSLT)
        TypeStatistic xslStyleSheets;
#endif
#if ENABLE(XBL)
        TypeStatistic xblDocs;
#endif
        TypeStatistic fonts;
        
        // 12/08/09 CSidhall - Added more stat info to export   
        uint32_t  capacity;         // In bytes the RAM cache size.
        uint32_t  liveSize;         // In bytes. Active or live size used.
		uint32_t  deadSize;         // In bytes. Inactive or dead size used (could be from a previous page).
        uint32_t  maxUsedSize;      // In bytes.  Max RAM cache used (live + dead).
    };

    // The loader that fetches resources.
    Loader* loader() { return &m_loader; }

    // Request resources from the cache.  A load will be initiated and a cache object created if the object is not
    // found in the cache.
    CachedResource* requestResource(DocLoader*, CachedResource::Type, const KURL& url, const String& charset, bool isPreload = false);

    CachedCSSStyleSheet* requestUserCSSStyleSheet(DocLoader*, const String& url, const String& charset);
    
    // Sets the cache's memory capacities, in bytes. These will hold only approximately, 
    // since the decoded cost of resources like scripts and stylesheets is not known.
    //  - minDeadBytes: The maximum number of bytes that dead resources should consume when the cache is under pressure.
    //  - maxDeadBytes: The maximum number of bytes that dead resources should consume when the cache is not under pressure.
    //  - totalBytes: The maximum number of bytes that the cache should consume overall.
    void setCapacities(unsigned minDeadBytes, unsigned maxDeadBytes, unsigned totalBytes);

    // Turn the cache on and off.  Disabling the cache will remove all resources from the cache.  They may
    // still live on if they are referenced by some Web page though.
    void setDisabled(bool);
    bool disabled() const { return m_disabled; }
    
    void setPruneEnabled(bool enabled) { m_pruneEnabled = enabled; }
    void prune()
    {
        if (m_liveSize + m_deadSize <= m_capacity && m_deadSize <= m_maxDeadCapacity) // Fast path.
            return;
            
        pruneDeadResources(); // Prune dead first, in case it was "borrowing" capacity from live.
        pruneLiveResources();
    }

    // Remove an existing cache entry from both the resource map and from the LRU list.
    void remove(CachedResource*);

    void addDocLoader(DocLoader*);
    void removeDocLoader(DocLoader*);

    CachedResource* resourceForURL(const String&);

    // Calls to put the cached resource into and out of LRU lists.
    void insertInLRUList(CachedResource*);
    void removeFromLRUList(CachedResource*);

    // Called to adjust the cache totals when a resource changes size.
    void adjustSize(bool live, int delta);

    // Track decoded resources that are in the cache and referenced by a Web page.
    void insertInLiveDecodedResourcesList(CachedResource*);
    void removeFromLiveDecodedResourcesList(CachedResource*);

    void addToLiveResourcesSize(CachedResource*);
    void removeFromLiveResourcesSize(CachedResource*);

    // Function to collect cache statistics for the caches window in the Safari Debug menu.
    Statistics getStatistics();
    
    // 7/10/09 CSidhall - Added special image prune before decoder buffers are allocated
    bool pruneImages(unsigned additionalSize);
    bool pruneLiveImages(unsigned additionalSize);
    bool pruneDeadImages(unsigned additionalSize);

private:
    Cache();
  
// 4/27/09 CSidhall Activated cached destructor for memory leak fix
//- Old code: 
//    ~Cache(){}
//- New code    
    ~Cache();
//-

    LRUList* lruListFor(CachedResource*);
    void resourceAccessed(CachedResource*);
#ifndef NDEBUG
    void dumpLRULists(bool includeLive) const;
#endif

    unsigned liveCapacity() const;
    unsigned deadCapacity() const;
    
    void pruneDeadResources(); // Flush decoded and encoded data from resources not referenced by Web pages.
    void pruneLiveResources(); // Flush decoded data from resources still referenced by Web pages.

    // Member variables.
    HashSet<DocLoader*> m_docLoaders;
    Loader m_loader;

    bool m_disabled;  // Whether or not the cache is enabled.
    bool m_pruneEnabled;

    unsigned m_capacity;
    unsigned m_minDeadCapacity;
    unsigned m_maxDeadCapacity;

    unsigned m_liveSize; // The number of bytes currently consumed by "live" resources in the cache.
    unsigned m_deadSize; // The number of bytes currently consumed by "dead" resources in the cache.
    unsigned m_maxUsedSize; // Keep track of the largest size used

    // Size-adjusted and popularity-aware LRU list collection for cache objects.  This collection can hold
    // more resources than the cached resource map, since it can also hold "stale" muiltiple versions of objects that are
    // waiting to die when the clients referencing them go away.
    Vector<LRUList, 32> m_allResources;
    
    // List just for live resources with decoded data.  Access to this list is based off of painting the resource.
    LRUList m_liveDecodedResources;
    
    // A URL-based map of all resources that are in the cache (including the freshest version of objects that are currently being 
    // referenced by a Web page).
    HashMap<String, CachedResource*> m_resources;
};

// Function to obtain the global cache.
Cache* cache();

}

#endif
