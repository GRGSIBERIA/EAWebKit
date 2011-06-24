/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
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

#ifndef FrameLoader_h
#define FrameLoader_h

#include <wtf/FastAllocBase.h>
#include "CachedResource.h"
#include "CachePolicy.h"
#include "FormState.h"
#include "FrameLoaderTypes.h"
#include "KURL.h"
#include "StringHash.h"
#include "Timer.h"
#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>
#include <wtf/RefPtr.h>
#include "ResourceRequest.h"
#if USE(LOW_BANDWIDTH_DISPLAY)
#include "CachedResourceClient.h"
#endif

namespace KJS {
    class JSValue;
}

namespace WebCore {

    class Archive;
    class ArchiveResource;
    class AuthenticationChallenge;
    class CachedPage;
    class Document;
    class DocumentLoader;
    class Element;
    class Event;
    class FormData;
    class Frame;
    class FrameLoaderClient;
    class HistoryItem;
    class HTMLFormElement;
    class HTMLFrameOwnerElement;
    class IconLoader;
    class IntSize;
    class NavigationAction;
    class Node;
    class Page;
    class RenderPart;
    class ResourceError;
    class ResourceLoader;
    class ResourceRequest;
    class ResourceResponse;
    class SecurityOrigin;
    class SharedBuffer;
    class SubstituteData;
    class TextResourceDecoder;
    class Widget;

    struct FormSubmission;
    struct FrameLoadRequest;
    struct ScheduledRedirection;
    struct WindowFeatures;

    bool isBackForwardLoadType(FrameLoadType);

    typedef void (*NavigationPolicyDecisionFunction)(void* argument,
        const ResourceRequest&, PassRefPtr<FormState>, bool shouldContinue);
    typedef void (*NewWindowPolicyDecisionFunction)(void* argument,
        const ResourceRequest&, PassRefPtr<FormState>, const String& frameName, bool shouldContinue);
    typedef void (*ContentPolicyDecisionFunction)(void* argument, PolicyAction);

    class PolicyCheck: public WTF::FastAllocBase {
    public:
        PolicyCheck();

        void clear();
        void set(const ResourceRequest&, PassRefPtr<FormState>,
            NavigationPolicyDecisionFunction, void* argument);
        void set(const ResourceRequest&, PassRefPtr<FormState>, const String& frameName,
            NewWindowPolicyDecisionFunction, void* argument);
        void set(ContentPolicyDecisionFunction, void* argument);

        const ResourceRequest& request() const { return m_request; }
        void clearRequest();

        void call(bool shouldContinue);
        void call(PolicyAction);
        void cancel();

    private:
        ResourceRequest m_request;
        RefPtr<FormState> m_formState;
        String m_frameName;

        NavigationPolicyDecisionFunction m_navigationFunction;
        NewWindowPolicyDecisionFunction m_newWindowFunction;
        ContentPolicyDecisionFunction m_contentFunction;
        void* m_argument;
    };

    class FrameLoader : Noncopyable
#if USE(LOW_BANDWIDTH_DISPLAY)
    , private CachedResourceClient
#endif
    {
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
        FrameLoader(Frame*, FrameLoaderClient*);
        ~FrameLoader();

        //+ 4/28/09 CSidhall - Added for static cleanup on exit.
        static void staticFinalize();                             
        static HashSet<String, CaseFoldingHash>* s_pLocalSchemes;   // Moved from localSchemes() for access to static
        static HashSet<String, CaseFoldingHash>* getLocalSchemes(); 
        //- CS

        void init();

        Frame* frame() const { return m_frame; }

        // FIXME: This is not cool, people.
        void prepareForLoadStart();
        void setupForReplace();
        void setupForReplaceByMIMEType(const String& newMIMEType);
        void load(const KURL&, Event*);
        void load(const FrameLoadRequest&, bool lockHistory, bool userGesture,
            Event*, HTMLFormElement*, const HashMap<String, String>& formValues);
        void load(const KURL&, const String& referrer, FrameLoadType, const String& target,
            Event*, PassRefPtr<FormState>);
        void post(const KURL&, const String& referrer, const String& target,
            PassRefPtr<FormData>, const String& contentType,
            Event*, HTMLFormElement*, const HashMap<String, String>& formValues);



        void load(const ResourceRequest&);
        void load(const ResourceRequest&, const SubstituteData&);
        void load(const ResourceRequest&, const String& frameName);
        void load(const ResourceRequest&, const NavigationAction&, FrameLoadType, PassRefPtr<FormState>);
        
        void load(DocumentLoader*);
        void load(DocumentLoader*, FrameLoadType, PassRefPtr<FormState>);
        
        void loadURLIntoChildFrame(const KURL&, const String& referer, Frame*);
        void loadArchive(PassRefPtr<Archive> archive);

        bool applyLinkHooks(const ResourceRequest& );


        static bool canLoad(const KURL&, const String& referrer);
        static bool canLoad(const KURL&, const Document*);
        static bool canLoad(const CachedResource&, const Document*);
        static void reportLocalLoadFailed(Frame*, const String& url);

        static bool shouldHideReferrer(const KURL& url, const String& referrer);

        Frame* createWindow(FrameLoader* frameLoaderForFrameLookup, const FrameLoadRequest&, const WindowFeatures&, bool& created);

        unsigned long loadResourceSynchronously(const ResourceRequest&, ResourceError&, ResourceResponse&, Vector<char>& data);

        bool canHandleRequest(const ResourceRequest&);

        // Also not cool.
        void stopAllLoaders();
        void stopForUserCancel(bool deferCheckLoadComplete = false);

        bool isLoadingMainResource() const { return m_isLoadingMainResource; }
        bool isLoading() const;
        bool frameHasLoaded() const;

        int numPendingOrLoadingRequests(bool recurse) const;
        bool isReloading() const;
        String referrer() const;
        String outgoingReferrer() const;
        void loadEmptyDocumentSynchronously();

        DocumentLoader* activeDocumentLoader() const;
        DocumentLoader* documentLoader() const;
        DocumentLoader* policyDocumentLoader() const;
        DocumentLoader* provisionalDocumentLoader() const;
        FrameState state() const;
        static double timeOfLastCompletedLoad();
        
        void didReceiveAuthenticationChallenge(ResourceLoader*, const AuthenticationChallenge&);
        void didCancelAuthenticationChallenge(ResourceLoader*, const AuthenticationChallenge&);
        
        void assignIdentifierToInitialRequest(unsigned long identifier, const ResourceRequest&);
        void willSendRequest(ResourceLoader*, ResourceRequest&, const ResourceResponse& redirectResponse);
        void didReceiveResponse(ResourceLoader*, const ResourceResponse&);
        void didReceiveData(ResourceLoader*, const char*, int, int lengthReceived);
        void didFinishLoad(ResourceLoader*);
        void didFailToLoad(ResourceLoader*, const ResourceError&);
        const ResourceRequest& originalRequest() const;
        const ResourceRequest& initialRequest() const;
        void receivedMainResourceError(const ResourceError&, bool isComplete);
        void receivedData(const char*, int);

        void handleFallbackContent();
        bool isStopping() const;

        void finishedLoading();

        ResourceError cancelledError(const ResourceRequest&) const;
        ResourceError fileDoesNotExistError(const ResourceResponse&) const;
        ResourceError blockedError(const ResourceRequest&) const;
        ResourceError cannotShowURLError(const ResourceRequest&) const;

        void cannotShowMIMEType(const ResourceResponse&);
        ResourceError interruptionForPolicyChangeError(const ResourceRequest&);

        bool isHostedByObjectElement() const;
        bool isLoadingMainFrame() const;
        bool canShowMIMEType(const String& MIMEType) const;
        bool representationExistsForURLScheme(const String& URLScheme);
        String generatedMIMETypeForURLScheme(const String& URLScheme);

        void notifyIconChanged();

        void checkNavigationPolicy(const ResourceRequest&, NavigationPolicyDecisionFunction function, void* argument);
        void checkContentPolicy(const String& MIMEType, ContentPolicyDecisionFunction, void* argument);
        void cancelContentPolicyCheck();

        void reload();
        void reloadAllowingStaleData(const String& overrideEncoding);

        void didReceiveServerRedirectForProvisionalLoadForFrame();
        void finishedLoadingDocument(DocumentLoader*);
        void committedLoad(DocumentLoader*, const char*, int);
        bool isReplacing() const;
        void setReplacing();
        void revertToProvisional(DocumentLoader*);
        void setMainDocumentError(DocumentLoader*, const ResourceError&);
        void mainReceivedCompleteError(DocumentLoader*, const ResourceError&);
        bool subframeIsLoading() const;
        void willChangeTitle(DocumentLoader*);
        void didChangeTitle(DocumentLoader*);

        FrameLoadType loadType() const;

        void didFirstLayout();
        bool firstLayoutDone() const;

        void clientRedirectCancelledOrFinished(bool cancelWithLoadInProgress);
        void clientRedirected(const KURL&, double delay, double fireDate, bool lockHistory, bool isJavaScriptFormAction);
        bool shouldReload(const KURL& currentURL, const KURL& destinationURL);

        bool isQuickRedirectComing() const;

        void sendRemainingDelegateMessages(unsigned long identifier, const ResourceResponse&, int length, const ResourceError&);
        void requestFromDelegate(ResourceRequest&, unsigned long& identifier, ResourceError&);
        void loadedResourceFromMemoryCache(const CachedResource*);

        void recursiveCheckLoadComplete();
        void checkLoadComplete();
        void detachFromParent();
        void detachChildren();

        void addExtraFieldsToRequest(ResourceRequest&, bool isMainResource, bool alwaysFromRequest);

        FrameLoaderClient* client() const;

        void setDefersLoading(bool);

        void changeLocation(const String& url, const String& referrer, bool lockHistory = true, bool userGesture = false);
        void changeLocation(const KURL&, const String& referrer, bool lockHistory = true, bool userGesture = false);
        void urlSelected(const ResourceRequest&, const String& target, Event*, bool lockHistory, bool userGesture);
        void urlSelected(const FrameLoadRequest&, Event*, bool lockHistory, bool userGesture);
      
        bool requestFrame(HTMLFrameOwnerElement*, const String& url, const AtomicString& frameName);
        Frame* loadSubframe(HTMLFrameOwnerElement*, const KURL&, const String& name, const String& referrer);

        void submitForm(const char* action, const String& url, PassRefPtr<FormData>, const String& target, const String& contentType, const String& boundary, Event*);
        void submitFormAgain();
        void submitForm(const FrameLoadRequest&, Event*);

        void stop();
        void stopLoading(bool sendUnload);
        bool closeURL();

        void didExplicitOpen();

        KURL iconURL();
        void commitIconURLToIconDatabase(const KURL&);

        KURL baseURL() const;
        String baseTarget() const;
        KURL dataURLBaseFromRequest(const ResourceRequest& request) const;

        bool isScheduledLocationChangePending() const { return m_scheduledRedirection && isLocationChange(*m_scheduledRedirection); }
        void scheduleHTTPRedirection(double delay, const String& url);
        void scheduleLocationChange(const String& url, const String& referrer, bool lockHistory = true, bool userGesture = false);
        void scheduleRefresh(bool userGesture = false);
        void scheduleHistoryNavigation(int steps);

        bool canGoBackOrForward(int distance) const;
        void goBackOrForward(int distance);
        int getHistoryLength();
        KURL historyURL(int distance);

        void begin();
        void begin(const KURL&, bool dispatchWindowObjectAvailable = true, SecurityOrigin* forcedSecurityOrigin = 0);

        void write(const char* str, int len = -1, bool flush = false);
        void write(const String&);
        void end();
        void endIfNotLoadingMainResource();

        void setEncoding(const String& encoding, bool userChosen);
        String encoding() const;

        // Returns true if url is a JavaScript URL.
        bool executeIfJavaScriptURL(const KURL& url, bool userGesture = false, bool replaceDocument = true);

        KJS::JSValue* executeScript(const String& url, int baseLine, const String& script);
        KJS::JSValue* executeScript(const String& script, bool forceUserGesture = false);

        void gotoAnchor();
        bool gotoAnchor(const String& name); // returns true if the anchor was found
        void scrollToAnchor(const KURL&);

        void tokenizerProcessedData();

        void handledOnloadEvents();
        String userAgent(const KURL&) const;

        Widget* createJavaAppletWidget(const IntSize&, Element*, const HashMap<String, String>& args);

        void dispatchWindowObjectAvailable();
        void restoreDocumentState();

        Frame* opener();
        void setOpener(Frame*);
        bool openedByDOM() const;
        void setOpenedByDOM();

        void provisionalLoadStarted();

        bool userGestureHint();

        void resetMultipleFormSubmissionProtection();
        void didNotOpenURL(const KURL&);

        void addData(const char* bytes, int length);

        bool canCachePage();

        void checkCallImplicitClose();
        bool didOpenURL(const KURL&);

        void frameDetached();

        const KURL& url() const { return m_URL; }

        void updateBaseURLForEmptyDocument();

        void setResponseMIMEType(const String&);
        const String& responseMIMEType() const;

        bool containsPlugins() const;

        void loadDone();
        void finishedParsing();
        void checkCompleted();
        void scheduleCheckCompleted();
        void scheduleCheckLoadComplete();

        void clearRecordedFormValues();
        void recordFormValue(const String& name, const String& value, PassRefPtr<HTMLFormElement>);

        bool isComplete() const;

        bool requestObject(RenderPart* frame, const String& url, const AtomicString& frameName,
            const String& serviceType, const Vector<String>& paramNames, const Vector<String>& paramValues);

        KURL completeURL(const String& url);

        void didTellClientAboutLoad(const String& url);
        bool haveToldClientAboutLoad(const String& url);

        KURL originalRequestURL() const;

        void cancelAndClear();

        void setTitle(const String&);
        
        bool shouldTreatURLAsSameAsCurrent(const KURL&) const;

        void commitProvisionalLoad(PassRefPtr<CachedPage>);

        void goToItem(HistoryItem*, FrameLoadType);
        void saveDocumentAndScrollState();
        void saveScrollPositionAndViewStateToItem(HistoryItem*);

        // FIXME: These accessors are here for a dwindling number of users in WebKit, WebFrame
        // being the primary one.  After they're no longer needed there, they can be removed!
        HistoryItem* currentHistoryItem();
        HistoryItem* previousHistoryItem();
        HistoryItem* provisionalHistoryItem();
        void setCurrentHistoryItem(PassRefPtr<HistoryItem>);
        void setPreviousHistoryItem(PassRefPtr<HistoryItem>);
        void setProvisionalHistoryItem(PassRefPtr<HistoryItem>);

        void continueLoadWithData(SharedBuffer*, const String& mimeType, const String& textEncoding, const KURL&); 

        static void registerURLSchemeAsLocal(const String& scheme);
        static bool restrictAccessToLocal();
        static void setRestrictAccessToLocal(bool);
        static bool shouldTreatURLAsLocal(const String&);
        static bool shouldTreatSchemeAsLocal(const String&);

#if USE(LOW_BANDWIDTH_DISPLAY)    
        bool addLowBandwidthDisplayRequest(CachedResource*);
        void needToSwitchOutLowBandwidthDisplay() { m_needToSwitchOutLowBandwidthDisplay = true; }

        // Client can control whether to use low bandwidth display on a per frame basis.
        // However, this should only be used for the top frame, not sub-frame.
        void setUseLowBandwidthDisplay(bool lowBandwidth) { m_useLowBandwidthDisplay = lowBandwidth; }
        bool useLowBandwidthDisplay() const { return m_useLowBandwidthDisplay; }
#endif

        bool committingFirstRealLoad() const { return !m_creatingInitialEmptyDocument && !m_committedFirstRealDocumentLoad; }

        void iconLoadDecisionAvailable();

        bool shouldAllowNavigation(Frame* targetFrame) const;
        Frame* findFrameForNavigation(const AtomicString& name);

        void startIconLoader();

        void applyUserAgent(ResourceRequest& request);

    private:
        PassRefPtr<HistoryItem> createHistoryItem(bool useOriginal);
        PassRefPtr<HistoryItem> createHistoryItemTree(Frame* targetFrame, bool clipAtTarget);

        void addBackForwardItemClippedAtTarget(bool doClip);
        void restoreScrollPositionAndViewState();
        void saveDocumentState();
        void loadItem(HistoryItem*, FrameLoadType);
        bool urlsMatchItem(HistoryItem*) const;
        void invalidateCurrentItemCachedPage();
        void recursiveGoToItem(HistoryItem*, HistoryItem*, FrameLoadType);
        bool childFramesMatchItem(HistoryItem*) const;

        void updateHistoryForBackForwardNavigation();
        void updateHistoryForReload();
        void updateHistoryForStandardLoad();
        void updateHistoryForRedirectWithLockedHistory();
        void updateHistoryForClientRedirect();
        void updateHistoryForCommit();
        void updateHistoryForAnchorScroll();
    
        void redirectionTimerFired(Timer<FrameLoader>*);
        void checkCompletedTimerFired(Timer<FrameLoader>*);
        void checkLoadCompleteTimerFired(Timer<FrameLoader>*);
        
        void cancelRedirection(bool newLoadInProgress = false);

        void started();

        void completed();
        void parentCompleted();

        bool shouldUsePlugin(const KURL&, const String& mimeType, bool hasFallback, bool& useFallback);
        bool loadPlugin(RenderPart*, const KURL&, const String& mimeType,
        const Vector<String>& paramNames, const Vector<String>& paramValues, bool useFallback);
        
        bool loadProvisionalItemFromCachedPage();
        void cachePageForHistoryItem(HistoryItem*);

        void receivedFirstData();

        void updatePolicyBaseURL();
        void setPolicyBaseURL(const KURL&);

        // Also not cool.
        void stopLoadingSubframes();

        void clearProvisionalLoad();
        void markLoadComplete();
        void transitionToCommitted(PassRefPtr<CachedPage>);
        void frameLoadCompleted();

        void mainReceivedError(const ResourceError&, bool isComplete);

        void setLoadType(FrameLoadType);

        void checkNavigationPolicy(const ResourceRequest&, DocumentLoader*, PassRefPtr<FormState>,
                                   NavigationPolicyDecisionFunction, void* argument);
        void checkNewWindowPolicy(const NavigationAction&, const ResourceRequest&, 
                                  PassRefPtr<FormState>, const String& frameName);

        void continueAfterNavigationPolicy(PolicyAction);
        void continueAfterNewWindowPolicy(PolicyAction);
        void continueAfterContentPolicy(PolicyAction);
        void continueLoadAfterWillSubmitForm(PolicyAction = PolicyUse);

        static void callContinueLoadAfterNavigationPolicy(void*, const ResourceRequest&, PassRefPtr<FormState>, bool shouldContinue);
        void continueLoadAfterNavigationPolicy(const ResourceRequest&, PassRefPtr<FormState>, bool shouldContinue);
        static void callContinueLoadAfterNewWindowPolicy(void*, const ResourceRequest&, PassRefPtr<FormState>, const String& frameName, bool shouldContinue);
        void continueLoadAfterNewWindowPolicy(const ResourceRequest&, PassRefPtr<FormState>, const String& frameName, bool shouldContinue);
        static void callContinueFragmentScrollAfterNavigationPolicy(void*, const ResourceRequest&, PassRefPtr<FormState>, bool shouldContinue);
        void continueFragmentScrollAfterNavigationPolicy(const ResourceRequest&, bool shouldContinue);
        void addHistoryItemForFragmentScroll();

        void stopPolicyCheck();

        void closeDocument();
        
        void checkLoadCompleteForThisFrame();

        void setDocumentLoader(DocumentLoader*);
        void setPolicyDocumentLoader(DocumentLoader*);
        void setProvisionalDocumentLoader(DocumentLoader*);

        void setState(FrameState);

        void closeOldDataSources();
        void open(CachedPage&);
        void opened();
        void updateHistoryAfterClientRedirect();

        void clear(bool clearWindowProperties = true, bool clearScriptObjects = true);

        bool shouldReloadToHandleUnreachableURL(DocumentLoader*);
        void handleUnimplementablePolicy(const ResourceError&);

        void scheduleRedirection(ScheduledRedirection*);
        void startRedirectionTimer();
        void stopRedirectionTimer();

#if USE(LOW_BANDWIDTH_DISPLAY)
        // implementation of CachedResourceClient        
        virtual void notifyFinished(CachedResource*);

        void removeAllLowBandwidthDisplayRequests();    
        void switchOutLowBandwidthDisplayIfReady();        
#endif

        void dispatchDidCommitLoad();
        void dispatchAssignIdentifierToInitialRequest(unsigned long identifier, DocumentLoader*, const ResourceRequest&);
        void dispatchWillSendRequest(DocumentLoader*, unsigned long identifier, ResourceRequest&, const ResourceResponse& redirectResponse);
        void dispatchDidReceiveResponse(DocumentLoader*, unsigned long identifier, const ResourceResponse&);
        void dispatchDidReceiveContentLength(DocumentLoader*, unsigned long identifier, int length);
        void dispatchDidFinishLoading(DocumentLoader*, unsigned long identifier);

        static bool isLocationChange(const ScheduledRedirection&);

        Frame* m_frame;
        FrameLoaderClient* m_client;

        FrameState m_state;
        FrameLoadType m_loadType;

        RefPtr<DocumentLoader> m_documentLoader;
        RefPtr<DocumentLoader> m_provisionalDocumentLoader;
        RefPtr<DocumentLoader> m_policyDocumentLoader;

        FrameLoadType m_policyLoadType;
        PolicyCheck m_policyCheck;

        bool m_delegateIsHandlingProvisionalLoadError;
        bool m_delegateIsDecidingNavigationPolicy;
        bool m_delegateIsHandlingUnimplementablePolicy;

        bool m_firstLayoutDone;
        bool m_quickRedirectComing;
        bool m_sentRedirectNotification;
        bool m_inStopAllLoaders;
        bool m_navigationDuringLoad;

        String m_outgoingReferrer;

        CachePolicy m_cachePolicy;

        HashSet<String> m_urlsClientKnowsAbout;

        OwnPtr<FormSubmission> m_deferredFormSubmission;

        bool m_isExecutingJavaScriptFormAction;
        bool m_isRunningScript;

        String m_responseMIMEType;

        bool m_didCallImplicitClose;
        bool m_wasUnloadEventEmitted;
        bool m_isComplete;
        bool m_isLoadingMainResource;

        KURL m_URL;
        KURL m_workingURL;

        OwnPtr<IconLoader> m_iconLoader;
        bool m_mayLoadIconLater;

        bool m_cancellingWithLoadInProgress;

        OwnPtr<ScheduledRedirection> m_scheduledRedirection;

        bool m_needsClear;
        bool m_receivedData;

        bool m_encodingWasChosenByUser;
        String m_encoding;
        RefPtr<TextResourceDecoder> m_decoder;

        bool m_containsPlugIns;

        RefPtr<HTMLFormElement> m_formAboutToBeSubmitted;
        HashMap<String, String> m_formValuesAboutToBeSubmitted;
        KURL m_submittedFormURL;
    
        Timer<FrameLoader> m_redirectionTimer;
        Timer<FrameLoader> m_checkCompletedTimer;
        Timer<FrameLoader> m_checkLoadCompleteTimer;

        Frame* m_opener;
        HashSet<Frame*> m_openedFrames;

        bool m_openedByDOM;

        bool m_creatingInitialEmptyDocument;
        bool m_isDisplayingInitialEmptyDocument;
        bool m_committedFirstRealDocumentLoad;

        RefPtr<HistoryItem> m_currentHistoryItem;
        RefPtr<HistoryItem> m_previousHistoryItem;
        RefPtr<HistoryItem> m_provisionalHistoryItem;
        
        bool m_didPerformFirstNavigation;
        
#ifndef NDEBUG
        bool m_didDispatchDidCommitLoad;
#endif

#if USE(LOW_BANDWIDTH_DISPLAY)
        // whether to use low bandwidth dislay, set by client
        bool m_useLowBandwidthDisplay;

        // whether to call finishParsing() in switchOutLowBandwidthDisplayIfReady() 
        bool m_finishedParsingDuringLowBandwidthDisplay;

        // whether to call switchOutLowBandwidthDisplayIfReady;
        // true if there is external css, javascript, or subframe/plugin
        bool m_needToSwitchOutLowBandwidthDisplay;
        
        String m_pendingSourceInLowBandwidthDisplay;        
        HashSet<CachedResource*> m_externalRequestsInLowBandwidthDisplay;
#endif
    };

}

#endif
