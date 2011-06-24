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

#ifndef DOMApplicationCache_h
#define DOMApplicationCache_h

#if ENABLE(OFFLINE_WEB_APPLICATIONS)

#include <wtf/RefCounted.h>
#include "EventTarget.h"

#include "EventListener.h"
#include <wtf/HashMap.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class ApplicationCache;
class AtomicStringImpl;
class Frame;
class KURL;
class String;
    
class DOMApplicationCache : public RefCounted<DOMApplicationCache>, public EventTarget {
public:
    static PassRefPtr<DOMApplicationCache> create(Frame* frame) { return adoptRef(new DOMApplicationCache(frame)); }
    void disconnectFrame();

    enum Status {
        UNCACHED = 0,
        IDLE = 1,
        CHECKING = 2,
        DOWNLOADING = 3,
        UPDATEREADY = 4,
    };

    unsigned short status() const;
    
    void update(ExceptionCode&);
    void swapCache(ExceptionCode&);
    
    unsigned length() const;
    String item(unsigned item, ExceptionCode&);
    void add(const KURL&, ExceptionCode&);
    void remove(const KURL&, ExceptionCode&);

    virtual void addEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture);
    virtual void removeEventListener(const AtomicString& eventType, EventListener*, bool useCapture);
    virtual bool dispatchEvent(PassRefPtr<Event>, ExceptionCode&, bool tempEvent = false);
    
    typedef Vector<RefPtr<EventListener> > ListenerVector;
    typedef HashMap<AtomicStringImpl*, ListenerVector> EventListenersMap;
    EventListenersMap& eventListeners() { return m_eventListeners; }

    using RefCounted<DOMApplicationCache>::ref;
    using RefCounted<DOMApplicationCache>::deref;

    void setOnCheckingListener(PassRefPtr<EventListener> eventListener) { m_onCheckingListener = eventListener; }
    EventListener* onCheckingListener() const { return m_onCheckingListener.get(); }

    void setOnErrorListener(PassRefPtr<EventListener> eventListener) { m_onErrorListener = eventListener; }
    EventListener* onErrorListener() const { return m_onErrorListener.get(); }

    void setOnNoUpdateListener(PassRefPtr<EventListener> eventListener) { m_onNoUpdateListener = eventListener; }
    EventListener* onNoUpdateListener() const { return m_onNoUpdateListener.get(); }

    void setOnDownloadingListener(PassRefPtr<EventListener> eventListener) { m_onDownloadingListener = eventListener; }
    EventListener* onDownloadingListener() const { return m_onDownloadingListener.get(); }
    
    void setOnProgressListener(PassRefPtr<EventListener> eventListener) { m_onProgressListener = eventListener; }
    EventListener* onProgressListener() const { return m_onProgressListener.get(); }

    void setOnUpdateReadyListener(PassRefPtr<EventListener> eventListener) { m_onUpdateReadyListener = eventListener; }
    EventListener* onUpdateReadyListener() const { return m_onUpdateReadyListener.get(); }

    void setOnCachedListener(PassRefPtr<EventListener> eventListener) { m_onCachedListener = eventListener; }
    EventListener* onCachedListener() const { return m_onCachedListener.get(); }
    
    Frame* frame() const { return m_frame; }
    DOMApplicationCache* toDOMApplicationCache() { return this; }

    void callCheckingListener();
    void callErrorListener();    
    void callNoUpdateListener();    
    void callDownloadingListener();
    void callProgressListener();
    void callUpdateReadyListener();
    void callCachedListener();
    
private:
    DOMApplicationCache(Frame*);
    void callListener(const AtomicString& eventType, EventListener*);
    
    virtual void refEventTarget() { ref(); }
    virtual void derefEventTarget() { deref(); }

    ApplicationCache* associatedCache() const;
    bool swapCache();
    
    RefPtr<EventListener> m_onCheckingListener;
    RefPtr<EventListener> m_onErrorListener;
    RefPtr<EventListener> m_onNoUpdateListener;
    RefPtr<EventListener> m_onDownloadingListener;
    RefPtr<EventListener> m_onProgressListener;
    RefPtr<EventListener> m_onUpdateReadyListener;
    RefPtr<EventListener> m_onCachedListener;
    
    EventListenersMap m_eventListeners;

    Frame* m_frame;
};

} // namespace WebCore

#endif // ENABLE(OFFLINE_WEB_APPLICATIONS)

#endif // DOMApplicationCache_h
