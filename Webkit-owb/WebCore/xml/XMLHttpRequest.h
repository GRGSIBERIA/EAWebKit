/*
 *  Copyright (C) 2003, 2006, 2008 Apple Inc. All rights reserved.
 *  Copyright (C) 2005, 2006 Alexey Proskuryakov <ap@nypop.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*
* This file was modified by Electronic Arts Inc Copyright � 2009
*/

#ifndef XMLHttpRequest_h
#define XMLHttpRequest_h

#include "AccessControlList.h"
#include "EventListener.h"
#include "EventTarget.h"
#include "FormData.h"
#include "ResourceResponse.h"
#include "SubresourceLoaderClient.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

class Document;
class TextResourceDecoder;

class XMLHttpRequest : public RefCounted<XMLHttpRequest>, public EventTarget, private SubresourceLoaderClient {
public:
    static PassRefPtr<XMLHttpRequest> create(Document* document) { return adoptRef(new XMLHttpRequest(document)); }
    ~XMLHttpRequest();

    // These exact numeric values are important because JS expects them.
    enum State {
        UNSENT = 0,
        OPENED = 1,
        HEADERS_RECEIVED = 2,
        LOADING = 3,
        DONE = 4
    };

//+daw ca 23/07 static and global management
public:
    static HashSet<String, CaseFoldingHash>* forbiddenHeadersList();
    static void staticFinalize();

private:
    static int m_st_iIsEmpty;
//daw ca

public:
//+ 4/28/09 CSidhall - Added for leak fix on exit
    static HashSet<String, CaseFoldingHash>* s_pForbiddenHeadersBlackList;
    static HashSet<String, CaseFoldingHash>* getForbiddenHeadersBlackList();
//- CS



    virtual XMLHttpRequest* toXMLHttpRequest() { return this; }

    static void detachRequests(Document*);
    static void cancelRequests(Document*);

    String statusText(ExceptionCode&) const;
    int status(ExceptionCode&) const;
    State readyState() const;
    void open(const String& method, const KURL&, bool async, ExceptionCode&);
    void open(const String& method, const KURL&, bool async, const String& user, ExceptionCode&);
    void open(const String& method, const KURL&, bool async, const String& user, const String& password, ExceptionCode&);
    void send(ExceptionCode&);
    void send(Document*, ExceptionCode&);
    void send(const String&, ExceptionCode&);
    void abort();
    void setRequestHeader(const String& name, const String& value, ExceptionCode&);
    void overrideMimeType(const String& override);
    String getAllResponseHeaders(ExceptionCode&) const;
    String getResponseHeader(const String& name, ExceptionCode&) const;
    const KJS::UString& responseText() const;
    Document* responseXML() const;

    void setOnReadyStateChangeListener(PassRefPtr<EventListener> eventListener) { m_onReadyStateChangeListener = eventListener; }
    EventListener* onReadyStateChangeListener() const { return m_onReadyStateChangeListener.get(); }

    void setOnAbortListener(PassRefPtr<EventListener> eventListener) { m_onAbortListener = eventListener; }
    EventListener* onAbortListener() const { return m_onAbortListener.get(); }

    void setOnErrorListener(PassRefPtr<EventListener> eventListener) { m_onErrorListener = eventListener; }
    EventListener* onErrorListener() const { return m_onErrorListener.get(); }

    void setOnLoadListener(PassRefPtr<EventListener> eventListener) { m_onLoadListener = eventListener; }
    EventListener* onLoadListener() const { return m_onLoadListener.get(); }

    void setOnLoadStartListener(PassRefPtr<EventListener> eventListener) { m_onLoadStartListener = eventListener; }
    EventListener* onLoadStartListener() const { return m_onLoadStartListener.get(); }

    void setOnProgressListener(PassRefPtr<EventListener> eventListener) { m_onProgressListener = eventListener; }
    EventListener* onProgressListener() const { return m_onProgressListener.get(); }

	// static event handlers
	static void removeStaticEventListeners();

	static void setStaticOnAbortListener(PassRefPtr<EventListener> eventListener) { s_onAbortListener = eventListener; }
	static EventListener* staticOnAbortListener() { return s_onAbortListener.get(); }

	static void setStaticOnErrorListener(PassRefPtr<EventListener> eventListener) { s_onErrorListener = eventListener; }
	static EventListener* staticOnErrorListener() { return s_onErrorListener.get(); }

	static void setStaticOnLoadListener(PassRefPtr<EventListener> eventListener) { s_onLoadListener = eventListener; }
	static EventListener* staticOnLoadListener() { return s_onLoadListener.get(); }

	static void setStaticOnLoadStartListener(PassRefPtr<EventListener> eventListener) { s_onLoadStartListener = eventListener; }
	static EventListener* staticOnLoadStartListener() { return s_onLoadStartListener.get(); }

	static void setStaticOnProgressListener(PassRefPtr<EventListener> eventListener) { s_onProgressListener = eventListener; }
	static EventListener* staticOnProgressListener() { return s_onProgressListener.get(); }


    typedef Vector<RefPtr<EventListener> > ListenerVector;
    typedef HashMap<AtomicStringImpl*, ListenerVector> EventListenersMap;

    // useCapture is not used, even for add/remove pairing (for Firefox compatibility).
    virtual void addEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture);
    virtual void removeEventListener(const AtomicString& eventType, EventListener*, bool useCapture);
    virtual bool dispatchEvent(PassRefPtr<Event>, ExceptionCode&, bool tempEvent = false);
    EventListenersMap& eventListeners() { return m_eventListeners; }

    Document* document() const { return m_doc; }

    using RefCounted<XMLHttpRequest>::ref;
    using RefCounted<XMLHttpRequest>::deref;

private:
    XMLHttpRequest(Document*);
    
    virtual void refEventTarget() { ref(); }
    virtual void derefEventTarget() { deref(); }

    virtual void willSendRequest(SubresourceLoader*, ResourceRequest& request, const ResourceResponse& redirectResponse);
    virtual void didReceiveResponse(SubresourceLoader*, const ResourceResponse&);
    virtual void didReceiveData(SubresourceLoader*, const char* data, int size);
    virtual void didFail(SubresourceLoader*, const ResourceError&);
    virtual void didFinishLoading(SubresourceLoader*);
    virtual void receivedCancellation(SubresourceLoader*, const AuthenticationChallenge&);

    // Special versions for the preflight
    void didReceiveResponsePreflight(SubresourceLoader*, const ResourceResponse&);
    void didFinishLoadingPreflight(SubresourceLoader*);

    void processSyncLoadResults(const Vector<char>& data, const ResourceResponse&, ExceptionCode&);
    void updateAndDispatchOnProgress(unsigned int len);

    String responseMIMEType() const;
    bool responseIsXML() const;

    bool initSend(ExceptionCode&);

    String getRequestHeader(const String& name) const;
    void setRequestHeaderInternal(const String& name, const String& value);

    void changeState(State newState);
    void callReadyStateChangeListener();
    void dropProtection();
    void internalAbort();
    void clearResponse();
    void clearRequest();

    void createRequest(ExceptionCode&);

    void makeSameOriginRequest(ExceptionCode&);
    void makeCrossSiteAccessRequest(ExceptionCode&);

    void makeSimpleCrossSiteAccessRequest(ExceptionCode&);
    void makeCrossSiteAccessRequestWithPreflight(ExceptionCode&);
    void handleAsynchronousPreflightResult();

    void loadRequestSynchronously(ResourceRequest&, ExceptionCode&);
    void loadRequestAsynchronously(ResourceRequest&);

    bool isSimpleCrossSiteAccessRequest() const;
    String accessControlOrigin() const;

    void genericError();
    void networkError();
    void abortError();

    void dispatchReadyStateChangeEvent();
    void dispatchXMLHttpRequestProgressEvent(EventListener* listener, const AtomicString& type, bool lengthComputable, unsigned loaded, unsigned total);
    void dispatchAbortEvent();
    void dispatchErrorEvent();
    void dispatchLoadEvent();
    void dispatchLoadStartEvent();
    void dispatchProgressEvent(long long expectedLength);

    Document* m_doc;

	RefPtr<EventListener> m_onReadyStateChangeListener;
    RefPtr<EventListener> m_onAbortListener;
    RefPtr<EventListener> m_onErrorListener;
    RefPtr<EventListener> m_onLoadListener;
    RefPtr<EventListener> m_onLoadStartListener;
    RefPtr<EventListener> m_onProgressListener;

	// static event listeners
	static RefPtr<EventListener> s_onAbortListener;
	static RefPtr<EventListener> s_onErrorListener;
	static RefPtr<EventListener> s_onLoadListener;
	static RefPtr<EventListener> s_onLoadStartListener;
	static RefPtr<EventListener> s_onProgressListener;

    EventListenersMap m_eventListeners;

    KURL m_url;
    String m_method;
    HTTPHeaderMap m_requestHeaders;
    RefPtr<FormData> m_requestEntityBody;
    String m_mimeTypeOverride;
    bool m_async;

    RefPtr<SubresourceLoader> m_loader;
    State m_state;

    ResourceResponse m_response;
    String m_responseEncoding;

    RefPtr<TextResourceDecoder> m_decoder;
    unsigned long m_identifier;

    // Unlike most strings in the DOM, we keep this as a KJS::UString, not a WebCore::String.
    // That's because these strings can easily get huge (they are filled from the network with
    // no parsing) and because JS can easily observe many intermediate states, so it's very useful
    // to be able to share the buffer with JavaScript versions of the whole or partial string.
    // In contrast, this string doesn't interact much with the rest of the engine so it's not that
    // big a cost that it isn't a String.
    KJS::UString m_responseText;
    mutable bool m_createdDocument;
    mutable RefPtr<Document> m_responseXML;

    bool m_error;

    bool m_sameOriginRequest;
    bool m_allowAccess;
    bool m_inPreflight;
    HTTPHeaderMap m_crossSiteRequestHeaders;

    // FIXME: Add support for AccessControlList in a PI in an XML document in addition to the http header.
    OwnPtr<AccessControlList> m_httpAccessControlList;

    // Used for onprogress tracking
    long long m_receivedLength;
};

} // namespace WebCore

#endif // XMLHttpRequest_h
