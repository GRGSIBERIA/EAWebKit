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

#include "config.h"
#include "DOMApplicationCache.h"

#if ENABLE(OFFLINE_WEB_APPLICATIONS)

#include "ApplicationCache.h"
#include "ApplicationCacheGroup.h"
#include "DocumentLoader.h"
#include "Event.h"
#include "EventException.h"
#include "EventListener.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoader.h"

namespace WebCore {

using namespace EventNames;

DOMApplicationCache::DOMApplicationCache(Frame* frame)
    : m_frame(frame)
{
}

void DOMApplicationCache::disconnectFrame()
{
    m_frame = 0;
}

ApplicationCache* DOMApplicationCache::associatedCache() const
{
    if (!m_frame)
        return 0;
 
    return m_frame->loader()->documentLoader()->topLevelApplicationCache();
}

unsigned short DOMApplicationCache::status() const
{
    ApplicationCache* cache = associatedCache();    
    if (!cache)
        return UNCACHED;
    
    switch (cache->group()->status()) {
        case ApplicationCacheGroup::Checking:
            return CHECKING;
        case ApplicationCacheGroup::Downloading:
            return DOWNLOADING;
        case ApplicationCacheGroup::Idle: {
            if (cache != cache->group()->newestCache())
                return UPDATEREADY;
            
            return IDLE;
        }
        default:
            ASSERT_NOT_REACHED();
    }
    
    return 0;
}

void DOMApplicationCache::update(ExceptionCode& ec)
{
    ApplicationCache* cache = associatedCache();
    if (!cache) {
        ec = INVALID_STATE_ERR;
        return;
    }
    
    cache->group()->update(m_frame);
}

bool DOMApplicationCache::swapCache()
{
    if (!m_frame)
        return false;
    
    ApplicationCache* cache = m_frame->loader()->documentLoader()->applicationCache();
    if (!cache)
        return false;
    
    // Check if we already have the newest cache
    ApplicationCache* newestCache = cache->group()->newestCache();
    if (cache == newestCache)
        return false;
    
    ASSERT(cache->group() == newestCache->group());
    m_frame->loader()->documentLoader()->setApplicationCache(newestCache);
    
    return true;
}
    
void DOMApplicationCache::swapCache(ExceptionCode& ec)
{
    if (!swapCache())
        ec = INVALID_STATE_ERR;
}

unsigned DOMApplicationCache::length() const
{
    ApplicationCache* cache = associatedCache();
    if (!cache)
        return 0;
    
    return cache->numDynamicEntries();
}
    
String DOMApplicationCache::item(unsigned item, ExceptionCode& ec)
{
    ApplicationCache* cache = associatedCache();
    if (!cache) {
        ec = INVALID_STATE_ERR;
        return String();
    }

    if (item >= length()) {
        ec = INDEX_SIZE_ERR;
        return String();
    }
    
    return cache->dynamicEntry(item);
}
    
void DOMApplicationCache::add(const KURL& url, ExceptionCode& ec)
{
    ApplicationCache* cache = associatedCache();
    if (!cache) {
        ec = INVALID_STATE_ERR;
        return;
    }
 
    if (!url.isValid()) {
        ec = SYNTAX_ERR;
        return;
    }
        
    if (!cache->addDynamicEntry(url)) {
        // This should use the (currently not specified) security exceptions in HTML5 4.3.4
        ec = SECURITY_ERR;
    }
}

void DOMApplicationCache::remove(const KURL& url, ExceptionCode& ec)
{
    ApplicationCache* cache = associatedCache();
    if (!cache) {
        ec = INVALID_STATE_ERR;
        return;
    }
    
    cache->removeDynamicEntry(url);
}
    
void DOMApplicationCache::addEventListener(const AtomicString& eventType, PassRefPtr<EventListener> eventListener, bool)
{
    EventListenersMap::iterator iter = m_eventListeners.find(eventType.impl());
    if (iter == m_eventListeners.end()) {
        ListenerVector listeners;
        listeners.append(eventListener);
        m_eventListeners.add(eventType.impl(), listeners);
    } else {
        ListenerVector& listeners = iter->second;
        for (ListenerVector::iterator listenerIter = listeners.begin(); listenerIter != listeners.end(); ++listenerIter) {
            if (*listenerIter == eventListener)
                return;
        }
        
        listeners.append(eventListener);
        m_eventListeners.add(eventType.impl(), listeners);
    }    
}

void DOMApplicationCache::removeEventListener(const AtomicString& eventType, EventListener* eventListener, bool useCapture)
{
    EventListenersMap::iterator iter = m_eventListeners.find(eventType.impl());
    if (iter == m_eventListeners.end())
        return;
    
    ListenerVector& listeners = iter->second;
    for (ListenerVector::const_iterator listenerIter = listeners.begin(); listenerIter != listeners.end(); ++listenerIter) {
        if (*listenerIter == eventListener) {
            listeners.remove(listenerIter - listeners.begin());
            return;
        }
    }
}

bool DOMApplicationCache::dispatchEvent(PassRefPtr<Event> event, ExceptionCode& ec, bool tempEvent)
{
    if (event->type().isEmpty()) {
        ec = EventException::UNSPECIFIED_EVENT_TYPE_ERR;
        return true;
    }
    
    ListenerVector listenersCopy = m_eventListeners.get(event->type().impl());
    for (ListenerVector::const_iterator listenerIter = listenersCopy.begin(); listenerIter != listenersCopy.end(); ++listenerIter) {
        event->setTarget(this);
        event->setCurrentTarget(this);
        listenerIter->get()->handleEvent(event.get(), false);
    }
    
    return !event->defaultPrevented();
}

void DOMApplicationCache::callListener(const AtomicString& eventType, EventListener* listener)
{
    ASSERT(m_frame);
    
    RefPtr<Event> event = Event::create(eventType, false, false);
    if (listener) {
        event->setTarget(this);
        event->setCurrentTarget(this);
        listener->handleEvent(event.get(), false);
    }
    
    ExceptionCode ec = 0;
    dispatchEvent(event.release(), ec, false);
    ASSERT(!ec);    
}

void DOMApplicationCache::callCheckingListener()
{
    callListener(checkingEvent, m_onCheckingListener.get());
}

void DOMApplicationCache::callErrorListener()
{
    callListener(errorEvent, m_onErrorListener.get());
}

void DOMApplicationCache::callNoUpdateListener()
{
    callListener(noupdateEvent, m_onNoUpdateListener.get());
}

void DOMApplicationCache::callDownloadingListener()
{
    callListener(downloadingEvent, m_onDownloadingListener.get());
}

void DOMApplicationCache::callProgressListener()
{
    callListener(progressEvent, m_onProgressListener.get());
}

void DOMApplicationCache::callUpdateReadyListener()
{
    callListener(updatereadyEvent, m_onUpdateReadyListener.get());
}

void DOMApplicationCache::callCachedListener()
{
    callListener(cachedEvent, m_onCachedListener.get());
}

} // namespace WebCore

#endif // ENABLE(OFFLINE_WEB_APPLICATIONS)
