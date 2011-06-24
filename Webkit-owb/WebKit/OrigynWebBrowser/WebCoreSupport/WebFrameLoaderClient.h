/*
 * Copyright (C) 2008 Pleyo.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Pleyo nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PLEYO AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL PLEYO OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
* This file was modified by Electronic Arts Inc Copyright � 2009
*/

/////////////////////////////////////////////////////////////////////////////
// WebFrameLoaderClient.h
//
// Copyright (c) 2008, Electronic Arts Inc. All rights reserved.
// Modified by Paul Pedriana
/////////////////////////////////////////////////////////////////////////////

#ifndef WebFrameLoaderClient_h
#define WebFrameLoaderClient_h

#include <FrameLoaderClient.h>

#ifdef BENCH_LOAD_TIME
#include <sys/time.h>
#endif

namespace WebCore {
    class PluginView;
}

class WebFrame;
class WebHistory;

class WebFrameLoaderClient : public WebCore::FrameLoaderClient {
public:
    virtual ~WebFrameLoaderClient();
    virtual bool hasWebView() const;
    virtual bool hasFrameView() const;

    virtual void forceLayout();

    virtual void assignIdentifierToInitialRequest(unsigned long identifier, WebCore::DocumentLoader*, const WebCore::ResourceRequest&);

    virtual void dispatchWillSendRequest(WebCore::DocumentLoader*, unsigned long identifier, WebCore::ResourceRequest&, const WebCore::ResourceResponse& redirectResponse);
    virtual void dispatchDidReceiveAuthenticationChallenge(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::AuthenticationChallenge&);
    virtual void dispatchDidCancelAuthenticationChallenge(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::AuthenticationChallenge&);
    virtual void dispatchDidReceiveResponse(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::ResourceResponse&);
    virtual void dispatchDidReceiveContentLength(WebCore::DocumentLoader*, unsigned long identifier, int lengthReceived);
    virtual void dispatchDidFinishLoading(WebCore::DocumentLoader*, unsigned long identifier);
    virtual void dispatchDidFailLoading(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::ResourceError&);

    virtual void dispatchDidHandleOnloadEvents();
    virtual void dispatchDidReceiveServerRedirectForProvisionalLoad();
    virtual void dispatchDidCancelClientRedirect();
    virtual void dispatchWillPerformClientRedirect(const WebCore::KURL&, double interval, double fireDate);
    virtual void dispatchDidChangeLocationWithinPage();
    virtual void dispatchWillClose();
    virtual void dispatchDidReceiveIcon();
    virtual void dispatchDidStartProvisionalLoad();
    virtual void dispatchDidReceiveTitle(const WebCore::String&);
    virtual void dispatchDidCommitLoad();
    virtual void dispatchDidFinishDocumentLoad();
    virtual void dispatchDidFinishLoad();
    virtual void dispatchDidFirstLayout();

    virtual WebCore::Frame* dispatchCreatePage();
    virtual void dispatchShow();

    virtual void dispatchDidLoadMainResource(WebCore::DocumentLoader*);
    virtual void setMainDocumentError(WebCore::DocumentLoader*, const WebCore::ResourceError&);

    virtual void postProgressStartedNotification();
    virtual void postProgressEstimateChangedNotification();
    virtual void postProgressFinishedNotification();

    virtual void committedLoad(WebCore::DocumentLoader*, const char*, int);
    virtual void finishedLoading(WebCore::DocumentLoader*);

    virtual void updateGlobalHistory(const WebCore::KURL&);
    virtual bool shouldGoToHistoryItem(WebCore::HistoryItem*) const;

    virtual PassRefPtr<WebCore::DocumentLoader> createDocumentLoader(const WebCore::ResourceRequest&, const WebCore::SubstituteData&);
    virtual void setTitle(const WebCore::String& title, const WebCore::KURL&);

    virtual void savePlatformDataToCachedPage(WebCore::CachedPage*);
    virtual void transitionToCommittedForNewPage();

    virtual bool canCachePage() const;

    virtual PassRefPtr<WebCore::Frame> createFrame(const WebCore::KURL& url, const WebCore::String& name, WebCore::HTMLFrameOwnerElement* ownerElement,
                               const WebCore::String& referrer, bool allowsScrolling, int marginWidth, int marginHeight);
    virtual WebCore::Widget* createPlugin(const WebCore::IntSize&, WebCore::Element*, const WebCore::KURL&, const Vector<WebCore::String>&, const Vector<WebCore::String>&, const WebCore::String&, bool loadManually);
    virtual void redirectDataToPlugin(WebCore::Widget* pluginWidget);

protected:
    WebFrameLoaderClient(WebFrame*);

private:
    PassRefPtr<WebCore::Frame> createFrame(const WebCore::KURL&, const WebCore::String& name, WebCore::HTMLFrameOwnerElement*, const WebCore::String& referrer);
    void loadURLIntoChild(const WebCore::KURL&, const WebCore::String& referrer, WebFrame* childFrame);
    void receivedData(const char*, int, const WebCore::String&);
    WebHistory* webHistory() const;

    WebFrame* m_webFrame;

    // Points to the plugin view that data should be redirected to.
    WebCore::PluginView* m_pluginView;

    bool m_hasSentResponseToPlugin;
    int m_time;

#ifdef BENCH_LOAD_TIME
    struct timeval m_timerStart;
    struct timeval m_timerStop;
#endif

};

#endif // WebFrameLoaderClient_h
