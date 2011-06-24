/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2004, 2006, 2007, 2008 Apple Inc. All rights reserved.

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
*/

/*
* This file was modified by Electronic Arts Inc Copyright � 2009
*/

#ifndef loader_h
#define loader_h

#include "AtomicString.h"
#include "AtomicStringImpl.h"
#include "PlatformString.h"
#include "SubresourceLoaderClient.h"
#include "Timer.h"
#include <wtf/Deque.h>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/FastAllocBase.h>
namespace WebCore {

    class CachedResource;
    class DocLoader;
    class Request;

    class Loader : Noncopyable {
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
    public:
        Loader();
        ~Loader();

        void load(DocLoader*, CachedResource*, bool incremental = true, bool skipCanLoadCheck = false, bool sendResourceLoadCallbacks = true);

        void cancelRequests(DocLoader*);
        
        enum Priority { Low, Medium, High };
        void servePendingRequests(Priority minimumPriority = Low);

    private:
        Priority determinePriority(const CachedResource*) const;
        void scheduleServePendingRequests();
        
        void requestTimerFired(Timer<Loader>*);

        class Host : SubresourceLoaderClient,public WTF::FastAllocBase  {
        public:
            Host(const AtomicString& name, unsigned maxRequestsInFlight);
            ~Host();
            
            const AtomicString& name() const { return m_name; }
            void addRequest(Request*, Priority);
            void servePendingRequests(Priority minimumPriority = Low);
            void cancelRequests(DocLoader*);
            bool hasRequests() const;
        
        private:
            virtual void didReceiveResponse(SubresourceLoader*, const ResourceResponse&);
            virtual void didReceiveData(SubresourceLoader*, const char*, int);
            virtual void didFinishLoading(SubresourceLoader*);
            virtual void didFail(SubresourceLoader*, const ResourceError&);
            
            typedef Deque<Request*> RequestQueue;
            void servePendingRequests(RequestQueue& requestsPending);
            void didFail(SubresourceLoader*, bool cancelled = false);
            void cancelPendingRequests(RequestQueue& requestsPending, DocLoader*);
            
            RequestQueue m_requestsPending[High + 1];
            typedef HashMap<RefPtr<SubresourceLoader>, Request*> RequestMap;
            RequestMap m_requestsLoading;
            const AtomicString m_name;
            const int m_maxRequestsInFlight;
        };
        typedef HashMap<AtomicStringImpl*, Host*> HostMap;
        HostMap m_hosts;
        Host m_nonHTTPProtocolHost;
        
        Timer<Loader> m_requestTimer;
    };

}

#endif
