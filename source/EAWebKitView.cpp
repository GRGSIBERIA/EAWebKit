/*
Copyright (C) 2008-2009 Electronic Arts, Inc.  All rights reserved.

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
// EAWebKitView.cpp
// By Paul Pedriana
///////////////////////////////////////////////////////////////////////////////


#include <EAWebKit/EAWebKit.h>
#include <EAWebKit/EAWebKitGraphics.h>
#include <EAWebKit/EAWebkitSTLWrapper.h>
#include <EAWebKit/internal/EAWebKitEASTLHelpers.h>
#include <EAWebKit/internal/EAWebkitNodeListContainer.h>
#include <EAWebKit/internal/EAWebkitOverlaySurfaceArrayContainer.h>
#include <EAWebKit/EAWebKitFPUPrecision.h>
#include <EARaster/EARaster.h>
#ifdef  USE_EATHREAD_LIBRARY
    #include <EAThread/eathread_sync.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <Api/WebView.h>
#include <FrameView.h>
#include <FrameLoader.h>
#include <DocumentLoader.h>
#include <SubstituteData.h>
#include <ScriptController.h>
#include <Cursor.h>
#include <Cache.h>
#include <PageCache.h>
#include <FontCache.h>
#include <ResourceHandleManager.h>
#include "MainThread.h"
#include "SharedTimer.h"
#include <KeyboardEvent.h>
#include <FocusDirection.h>
#include <page.h>
#include <FocusController.h>
#include <EASTL/vector.h>

#include <EAWebKit/internal/InputBinding/EAWebKitDocumentNavigator.h>
#include <EAWebKit/internal/InputBinding/EAWebKitEventListener.h>
#include <EAWebKit/internal/InputBinding/EAWebKitPolarRegion.h>
#include <EAWebKit/internal/InputBinding/EAWebKitUtils.h>
#include "HTMLInputElement.h"
#include <NodeList.h>
#include <EventNames.h>
#include <EAWebKit/internal/InputBinding/EAWebKitDOMWalker.h>
#include <EAWebKit/internal/EAWebKitAssert.h>
#include <HTMLInputElement.h>
#include <HTMLNames.h>
#include <EAWebKit/internal/EAWebKitString.h>
#include "debugger.h"
#include "ExecState.h"
#include "DebuggerCallFrame.h"
#include "JSDOMWindow.h"
#include "c_runtime.h"
#include "BAL/OWBAL/Concretizations/Types/Common/BCbal_objectCommon.h"
#include "WebCore/bridge/bal/bal_class.h"
#include "HTMLTextAreaElement.h"
#include "xml/XMLHttpRequest.h"
#include <EAWebKit/internal/EAWebkitJavascriptBinding.h>


namespace EA
{

namespace WebKit
{

    LoadInfo::LoadInfo()
		: mpView(NULL),
		mLET(kLETNone),
		mbStarted(false),
		mbCompleted(false),
		mContentLength(-1),
		mProgressEstimation(0.0),
		mURI(),
		mPageTitle(),
        mLastChangedTime(0)
	{
	}


///////////////////////////////////////////////////////////////////////
// View Notification
///////////////////////////////////////////////////////////////////////


ViewNotification* gpViewNotification = NULL;
XMLHttpRequestEventListener* gpEventListener = NULL;


EAWEBKIT_API void SetViewNotification(ViewNotification* pViewNotification)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	gpViewNotification = pViewNotification;

	if (pViewNotification)
	{
		gpEventListener = new XMLHttpRequestEventListener(pViewNotification);

		WebCore::XMLHttpRequest::setStaticOnAbortListener(gpEventListener); 
		WebCore::XMLHttpRequest::setStaticOnErrorListener(gpEventListener);
		WebCore::XMLHttpRequest::setStaticOnLoadListener(gpEventListener);
		WebCore::XMLHttpRequest::setStaticOnLoadStartListener(gpEventListener);
		WebCore::XMLHttpRequest::setStaticOnProgressListener(gpEventListener);
	}
	
}


EAWEBKIT_API ViewNotification* GetViewNotification()
{
    return gpViewNotification;
}


TextInputStateInfo::TextInputStateInfo()
 : mIsActivated(false),
   mIsPasswordField(false),
   mIsSearchField(false)
{
}   
// Constructors for the metrics callback system
ViewProcessInfo::ViewProcessInfo()
 : mProcessType(kVProcessTypeNone),
   mProcessStatus(kVProcessStatusNone),
   mStartTime(0.0),
   mIntermediateTime(0.0),
   mURI(0),
   mSize(0),
   mJobId(0)
{
}

ViewProcessInfo::ViewProcessInfo(VProcessType type,VProcessStatus status)
 : mProcessType(type),
   mProcessStatus(status),
   mStartTime(0.0),
   mIntermediateTime(0.0),
   mURI(0),
   mSize(0),
   mJobId(0)
{
}

void ViewProcessInfo::ResetTime()
{
	mStartTime = 0.0;
	mIntermediateTime = 0.0;
}

//Global array for profile process.
ViewProcessInfo gProcessInfoArray[kVProcessTypeLast];

void NOTIFY_PROCESS_STATUS(VProcessType processType, VProcessStatus processStatus)
{
	const Parameters& parameters = EA::WebKit::GetParameters();
	if(!parameters.mbEnableProfiling)
		return;

	EAW_ASSERT_MSG(processType<kVProcessTypeLast, "Size mismatch. The process enum is higher than the size of the array.");
	if(processType<kVProcessTypeLast)
	{
		gProcessInfoArray[processType].mProcessType = processType; //lame but works(since we don't initialize the global array with each process type.
		gProcessInfoArray[processType].mProcessStatus = processStatus;
		
		if(processStatus == kVProcessStatusStarted)
			gProcessInfoArray[processType].ResetTime();
		
		EA::WebKit::ViewNotification* const pViewNotification = EA::WebKit::GetViewNotification();
		if(pViewNotification)
			pViewNotification->ViewProcessStatus(gProcessInfoArray[processType]);

	}
}

void NOTIFY_PROCESS_STATUS(ViewProcessInfo& process, VProcessStatus processStatus)
{
	const Parameters& parameters = EA::WebKit::GetParameters();
	if(!parameters.mbEnableProfiling)
		return;

	if(processStatus == kVProcessStatusStarted)
		process.ResetTime();

	process.mProcessStatus = processStatus;
	
	EA::WebKit::ViewNotification* const pViewNotification = EA::WebKit::GetViewNotification();
	if(pViewNotification)
		pViewNotification->ViewProcessStatus(process);
}
///////////////////////////////////////////////////////////////////////
// View
///////////////////////////////////////////////////////////////////////

typedef eastl::fixed_vector<View*, 8> ViewPtrArray;
ViewPtrArray gViewPtrArray;


EAWEBKIT_API int GetViewCount()
{
#ifdef USE_EATHREAD_LIBRARY
    // To do: Use a mutex instead of simply memory barriers.
    EAReadBarrier();
#endif

    return (int)gViewPtrArray.size();
}

EAWEBKIT_API View* GetView(int index)
{
#ifdef USE_EATHREAD_LIBRARY    
    // To do: Use a mutex instead of simply memory barriers.
    EAReadBarrier();
#endif

    if(index < (int)gViewPtrArray.size())
        return gViewPtrArray[(eastl_size_t)index];

    return NULL;
}

EAWEBKIT_API bool IsViewValid(View* pView)
{
#ifdef USE_EATHREAD_LIBRARY    
    // To do: Use a mutex instead of simply memory barriers.
    EAReadBarrier();
#endif

    // This is a linear search, but the list size is usually tiny.
    for(eastl_size_t i = 0, iEnd = gViewPtrArray.size(); i < iEnd; ++i)
    {
        if(pView == gViewPtrArray[i])
            return true;
    }

    return false;
}


EAWEBKIT_API View* GetView(::WebView* pWebView)
{
    // To do: Use a mutex instead of simply memory barriers.
    // Instead of doing a loop, we could possibly stuff a pointer into the given WebView directly or indirectly.
    for(eastl_size_t i = 0, iEnd = gViewPtrArray.size(); i < iEnd; ++i)
    {
        View*    const pViewCurrent    = gViewPtrArray[i];
        WebView* const pWebViewCurrent = pViewCurrent->GetWebView();

        if(pWebViewCurrent == pWebView)
            return pViewCurrent;
    }

    return NULL;
}


EAWEBKIT_API View* GetView(::WebFrame* pWebFrame)
{
    // To do: Use a mutex instead of simply memory barriers.
    // Instead of doing a loop, we could possibly stuff a pointer into the given WebFrame directly or indirectly.
    // To consider: This function isn't strictly needed, as we could get along with just the WebView version of it.
    for(eastl_size_t i = 0, iEnd = gViewPtrArray.size(); i < iEnd; ++i)
    {
        View*     const pViewCurrent     = gViewPtrArray[i];
        WebFrame* const pWebFrameCurrent = pViewCurrent->GetWebFrame();

        if(pWebFrameCurrent == pWebFrame)
            return pViewCurrent;
    }

    return NULL;
}


EAWEBKIT_API View* GetView(WebCore::Frame* pFrame)
{
    if(pFrame)
    {
        WebCore::Page* pPage = pFrame->page();
        if(pPage)
        {
            ::WebView* pWebView = ::kit(pPage);  //From #include <WebView.h>
            if(pWebView)
            {
                EA::Raster::Surface* pSurface = pWebView->viewWindow();
                if(pSurface)
                {
                    return static_cast<View*>(pSurface->mpUserData);
                }
            }
        }
    }
   return NULL;
}


EAWEBKIT_API View* GetView(WebCore::FrameView* pFrameView)
{
    if(pFrameView)
        return GetView(pFrameView->frame());

    return NULL;
}

EAWEBKIT_API View* CreateView() 
{
	return WTF::fastNew<View>();
}

EAWEBKIT_API void DestroyView(View* pView)
{
	WTF::fastDelete<View>(pView);
}

const float PI_4 = 3.14159f / 4.0f;

View::View()
  : mpWebView(),
    mpSurface(),
    mViewParameters(),
    mLoadInfo(),
    mCursorPos(0, 0),
    mpModalInputClient(),
	mOverlaySurfaceArrayContainer(0),
    mLinkHookManager(this),
    mTextInputStateInfo(),
	mDebugger(0),
	mNodeListContainer(0),
	mBestNodeX(0),
	mBestNodeY(0),
	mBestNodeWidth(0),
	mBestNodeHeight(0),
	mCachedNavigationUpId(),
	mCachedNavigationDownId(),
	mCachedNavigationLeftId(),
	mCachedNavigationRightId(),
	mNavigatorTheta(PI_4),
	mURI(),
	mJavascriptBindingObject(0),
    mJavascriptBindingObjectName(0)
{
    gViewPtrArray.push_back(this);
	mNodeListContainer = WTF::fastNew<NodeListContainer> ();
	mOverlaySurfaceArrayContainer = WTF::fastNew<OverlaySurfaceArrayContainer> ();
}

View::~View()
{
    gViewPtrArray.erase(eastl::find(gViewPtrArray.begin(), gViewPtrArray.end(), this));

	if (mDebugger)
	{
		delete mDebugger;
	}

	if(mNodeListContainer)
	{
		WTF::fastDelete<NodeListContainer>(mNodeListContainer);
		mNodeListContainer = 0;
	}
	if(mOverlaySurfaceArrayContainer)
	{
		WTF::fastDelete<OverlaySurfaceArrayContainer>(mOverlaySurfaceArrayContainer);
		mOverlaySurfaceArrayContainer = 0;
	}

    View::ShutdownView();
}


bool View::InitView(const ViewParameters& vp)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	
	mViewParameters = vp;

    if(!mpSurface)
        mpSurface = EA::Raster::CreateSurface(vp.mWidth, vp.mHeight, EA::Raster::kPixelFormatTypeARGB);

    if(mpSurface)
    {
        mpWebView = WebView::createInstance();

        if(mpWebView)
        {
            const Parameters& parameters = EA::WebKit::GetParameters();

            const WKAL::IntRect   viewRect(0, 0, vp.mWidth, vp.mHeight);
            const WebCore::String frameName(parameters.mpApplicationName ? parameters.mpApplicationName : "EAWebKit");
            const WebCore::String groupName;

            mpWebView->parseConfigFile();
            mpWebView->initWithFrame(viewRect, frameName, groupName);
            mpWebView->setViewWindow(mpSurface);
            mpWebView->setProhibitsMainFrameScrolling(!vp.mbScrollingEnabled);
            mpWebView->setTabKeyCyclesThroughElements(vp.mbTabKeyFocusCycle);
            if(parameters.mpUserAgent)
                mpWebView->setCustomUserAgent(WebCore::String(parameters.mpUserAgent));

            if(parameters.mpApplicationName)
                mpWebView->setApplicationNameForUserAgent(WebCore::String(parameters.mpApplicationName));

            // We store a pointer to 'this' in the Surface.
            mpSurface->mpUserData = static_cast<View*>(this);
            
            mpWebView->setTranparentBackground(vp.mbTransparentBackground); // 7/23/09 CSidhall - Added storing of transparent background
            // Apply the setting to the current frame view.
            if(vp.mbTransparentBackground)
            {
                WebCore::FrameView* pFrameView = GetFrameView();
                EAW_ASSERT(pFrameView);

                pFrameView->setTransparent(vp.mbTransparentBackground);
            }
        }
        else
            Shutdown();
    }

    return (mpSurface != NULL) && (mpWebView != NULL);
}


void View::ShutdownView()
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	KJS::SetCollectorStackBase();   // 9/4/09 - CSidhall - Added to get approximate stack base

    WebCore::XMLHttpRequest::removeStaticEventListeners();
	delete gpEventListener;
	gpEventListener = NULL;

	if (mJavascriptBindingObject)
	{
		delete mJavascriptBindingObject;
        mJavascriptBindingObject = NULL;	
        KJS::Bindings::BalClass::cleanup();
	}
	

    SetModalInput(NULL);

    if(mpWebView)
    {
        delete mpWebView;
        mpWebView = NULL;
    }

    if(mpSurface)
    {
        EA::Raster::DestroySurface(mpSurface);
        mpSurface = NULL;
    }
}


void View::GetSize(int& w, int& h) const
{
    if(mpSurface)
    {
        w = mpSurface->mWidth;
        h = mpSurface->mHeight;
    }
    else
    {
        w = 0;
        h = 0;
    }
}


bool View::SetSize(int w, int h)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   

	bool bResult = true;

    // Only recreate surfaces if there is a change.
    if(!mpSurface || (mpSurface->mWidth != w) || (mpSurface->mHeight != h))
    {
        if(mpSurface)
            bResult = mpSurface->Resize(w, h, false);
        else
        {
            mpSurface = EA::Raster::CreateSurface(w, h, EA::Raster::kPixelFormatTypeARGB);
            bResult   = (mpSurface != NULL);
        }
    }

    EAW_ASSERT(mpWebView); // If this fails, are you calling SetSize before calling InitView?
    if(mpWebView)
    {
        mpWebView->setViewWindow(mpSurface);

        BalResizeEvent resizeEvent = { w, h };
        mpWebView->onResize(resizeEvent);
    }

	return bResult;
}


bool View::SetURI(const char* pURI)
{
    // Note that by design we do not try to 'fix' the URI by doing things like prepending "http://" in front
    // of it. The reason for this is that the user may be using a custom URI scheme that we are not aware of.
    // It is the job of the higher layer to fix URIs if they want to.
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   

	if(mpWebView && pURI && pURI[0])
    {
        ::WebFrame* pWebFrame = mpWebView->topLevelFrame();
        EAW_ASSERT(pWebFrame);

        double timeoutSeconds = (double)EA::WebKit::GetParameters().mPageTimeoutSeconds;

		
        pWebFrame->loadURL(pURI, timeoutSeconds);
		return true;
    }

    EAW_FAIL_MSG("View::SetURI failure. Have you called View::InitView yet?");
	return false; 
}

void View::Refresh()
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	if(mpWebView)
	{
		::WebFrame* pWebFrame = mpWebView->topLevelFrame();
		EAW_ASSERT(pWebFrame);

		const double timeoutSeconds = (double)EA::WebKit::GetParameters().mPageTimeoutSeconds;
		EAW_ASSERT(timeoutSeconds > 0.0);

		pWebFrame->loadURL(pWebFrame->url(), timeoutSeconds);
	}
}


bool View::LoadResourceRequest(const WebCore::ResourceRequest& resourceRequest)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	if(mpWebView)
    {
		::WebFrame* pWebFrame = mpWebView->topLevelFrame();
        EAW_ASSERT(pWebFrame);
        pWebFrame->loadRequest(resourceRequest);
        
		return true;
    }

    return false;
}
const char16_t* View::GetURI()
{
    WebCore::Frame* pFrame = GetFrame();

    if(pFrame)
    {
        const WebCore::KURL&   url  = pFrame->loader()->url();
        const WebCore::String& sURL = url.string();

        GET_FIXEDSTRING16(mURI)->assign(sURL.characters(), sURL.length());
		return GET_FIXEDSTRING16(mURI)->c_str();
    }

	return NULL;
  
}

bool View::SetHTML(const char* pHTML, size_t length, const char* pBaseURL)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	return SetContent(pHTML, length, "text/html", "utf-8", pBaseURL);
}


bool View::SetContent(const void* pData, size_t length, const char* pMimeType, const char* pEncoding, const char* pBaseURL)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	WebCore::Frame* pFrame = GetFrame();

    if(pFrame)
    {
		WebCore::KURL                      kurl(pBaseURL); // It handles the case of pBaseURL == NULL.
        WebCore::ResourceRequest           request(kurl);
        WTF::RefPtr<WebCore::SharedBuffer> buffer(WebCore::SharedBuffer::create((const char*)pData, length));
        const WebCore::String              sMimeType(pMimeType ? pMimeType : "text/html");
        const WebCore::String              sEncoding(pEncoding ? pEncoding : "");
        WebCore::SubstituteData            substituteData(buffer, sMimeType, sEncoding, kurl);

        pFrame->loader()->load(request, substituteData);

		return true;
    }

    return false;
}


void View::CancelLoad()
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	if(GetFrame() && GetFrame()->loader())
	{
		GetFrame()->loader()->stopAllLoaders();
	}
}


bool View::GoBack()
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	if(mpWebView)
        return mpWebView->goBack();
    return false;
}


bool View::GoForward()
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	if(mpWebView)
        return mpWebView->goForward();
    return false;
}


LoadInfo& View::GetLoadInfo()
{
    return mLoadInfo;
}
                          
TextInputStateInfo& View::GetTextInputStateInfo()
{
    return mTextInputStateInfo;
}


EA::WebKit::JavascriptValue View::EvaluateJavaScript(const char* pScriptSource, size_t length)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	// For a more general solution, which we may want to look into, see:
    //     https://lists.webkit.org/pipermail/webkit-dev/2008-November/005686.html

    WebCore::Frame*            pFrame = GetFrame();
    WebCore::ScriptController* pProxy = pFrame->script();

	EA::WebKit::JavascriptValue returnValue;
	returnValue.SetUndefined();

    if(pProxy)
    {
        const WebCore::String sFileName;
        const int             baseLine = 1;
        const WebCore::String sScriptSource(pScriptSource, length);

        KJS::JSValue* pValue = pProxy->evaluate(sFileName, baseLine, sScriptSource);

        if(pValue)
        {
			if (pValue->isBoolean())
			{
				returnValue.SetBooleanValue(pValue->getBoolean());
			}
			else if (pValue->isUndefinedOrNull())
			{
				returnValue.SetUndefined();
			}
			else if (pValue->isNumber())
			{
				returnValue.SetNumberValue(pValue->uncheckedGetNumber());
			}
			else if (pValue->isString())
			{
				// This might not  be null-terminated
				returnValue.SetStringValue(pValue->getString().data());
			}
            // Do we need to free pValue in some way?
        }
    }



    return returnValue;
}


bool View::Tick()
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	
	KJS::SetCollectorStackBase();   // 9/2/09 - CSidhall - Added to get approximate stack base

    // Notify tick start callback (this is already known by the user but groups it with other profile calls)
    NOTIFY_PROCESS_STATUS(EA::WebKit::kVProcessTypeViewTick, EA::WebKit::kVProcessStatusStarted);

	WTF::dispatchFunctionsFromMainThread();

	NOTIFY_PROCESS_STATUS(EA::WebKit::kVProcessTypeTransportTick, EA::WebKit::kVProcessStatusStarted);
	
	WebCore::ResourceHandleManager* pRHM = WebCore::ResourceHandleManager::sharedInstance();
	EAW_ASSERT(pRHM);
	if(pRHM)
		pRHM->TickTransportHandlers();
	
	NOTIFY_PROCESS_STATUS(EA::WebKit::kVProcessTypeTransportTick, EA::WebKit::kVProcessStatusEnded);

    WebCore::fireTimerIfNeeded();

    // Check for a dirty FrameView, and if so then trigger a repaint.
    // The repaint will result in any notifications being sent out.
    WebCore::FrameView*     const pFrameView  = GetFrameView();
    EA::WebKit::Parameters& parameters        = EA::WebKit::GetParameters();

    if(pFrameView && mpWebView && pFrameView->IsDirty()) // If there is anything to draw...
    {
        if(parameters.mbDrawIntermediatePages || !mpWebView->isLoading())  // If we are to draw intermediate pages (as well as completed pages) or if the view has already completed loading...
        {
            // Notify Draw start Process callback
            NOTIFY_PROCESS_STATUS(EA::WebKit::kVProcessTypeDraw, EA::WebKit::kVProcessStatusStarted);

            BalEventExpose exposeEvent;
            mpWebView->onExpose(exposeEvent);

  			NOTIFY_PROCESS_STATUS(EA::WebKit::kVProcessTypeDraw, EA::WebKit::kVProcessStatusEnded);
        }
    }

    // Notify tick end callback (this is already known by the user but groups it with other profile calls)
	NOTIFY_PROCESS_STATUS(EA::WebKit::kVProcessTypeViewTick, EA::WebKit::kVProcessStatusEnded);

	return true;
}


void View::RedrawArea(int x, int y, int w, int h)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	if(!mpWebView)
		return;
    
	// This function triggers a WebKit-level redraw the of view (i.e. HTML) in an area.
    if((w == 0) || (h == 0))
    {
        x = 0;
        y = 0;
        w = mpSurface->mWidth;
        h = mpSurface->mHeight;
    }

    const WKAL::IntRect rect(x, y, w, h);
    mpWebView->addToDirtyRegion(rect);

    BalEventExpose exposeEvent;
    mpWebView->onExpose(exposeEvent);

}


void View::ViewUpdated(int x, int y, int w, int h)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   

	// Since the view surface has been updated, we will need to redraw any existing
    // overlay surfaces (e.g. overlaid popup modal menus).
    BlitOverlaySurfaces();

    // Notify the user of the update.
    EA::WebKit::ViewNotification* pVN = EA::WebKit::GetViewNotification();

    if(pVN)
    {
        EA::WebKit::ViewUpdateInfo vui = { this, x, y, w, h };
        pVN->ViewUpdate(vui);
    }

}


void View::Scroll(int x, int y)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	NOTIFY_PROCESS_STATUS(EA::WebKit::kVProcessTypeScrollEvent, EA::WebKit::kVProcessStatusStarted);
	
	if(mpWebView)
        mpWebView->scrollBy(WebCore::IntPoint(x, y));

	NOTIFY_PROCESS_STATUS(EA::WebKit::kVProcessTypeScrollEvent, EA::WebKit::kVProcessStatusEnded);
}

void View::GetScrollOffset(int& x, int& y)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	if(mpWebView)
	{
		const WebCore::IntPoint point = mpWebView->scrollOffset();
		x = point.x();
		y = point.y();
	}
	else
	{
		x = 0;
		y = 0;
	}
}


void View::OnKeyboardEvent(const KeyboardEvent& keyboardEvent)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	NOTIFY_PROCESS_STATUS(EA::WebKit::kVProcessTypeKeyBoardEvent, EA::WebKit::kVProcessStatusStarted);
	
	if(mpModalInputClient)
        mpModalInputClient->OnKeyboardEvent(keyboardEvent);
    else if(mpWebView)
    {
        if(keyboardEvent.mbDepressed)
            mpWebView->onKeyDown(keyboardEvent);
        else
            mpWebView->onKeyUp(keyboardEvent);
    }

	NOTIFY_PROCESS_STATUS(EA::WebKit::kVProcessTypeKeyBoardEvent, EA::WebKit::kVProcessStatusEnded);
}


void View::OnMouseMoveEvent(const MouseMoveEvent& mouseMoveEvent)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   

	NOTIFY_PROCESS_STATUS(EA::WebKit::kVProcessTypeMouseMoveEvent, EA::WebKit::kVProcessStatusStarted);
	
	if(mpModalInputClient)
        mpModalInputClient->OnMouseMoveEvent(mouseMoveEvent);
    else if(mpWebView)
        mpWebView->onMouseMotion(mouseMoveEvent);

    mCursorPos.set(mouseMoveEvent.mX, mouseMoveEvent.mY);

	NOTIFY_PROCESS_STATUS(EA::WebKit::kVProcessTypeMouseMoveEvent, EA::WebKit::kVProcessStatusEnded);
}


void View::OnMouseButtonEvent(const MouseButtonEvent& mouseButtonEvent)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	NOTIFY_PROCESS_STATUS(EA::WebKit::kVProcessTypeMouseButtonEvent, EA::WebKit::kVProcessStatusStarted);
	
	if(mpModalInputClient)
        mpModalInputClient->OnMouseButtonEvent(mouseButtonEvent);
    else if(mpWebView)
    {
        if(mouseButtonEvent.mbDepressed)
            mpWebView->onMouseButtonDown(mouseButtonEvent);
        else
            mpWebView->onMouseButtonUp(mouseButtonEvent);
    }

	NOTIFY_PROCESS_STATUS(EA::WebKit::kVProcessTypeMouseButtonEvent, EA::WebKit::kVProcessStatusEnded);

}


void View::OnMouseWheelEvent(const MouseWheelEvent& mouseWheelEvent)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	NOTIFY_PROCESS_STATUS(EA::WebKit::kVProcessTypeMouseWheelEvent, EA::WebKit::kVProcessStatusStarted);
		
	if(mpModalInputClient)
        mpModalInputClient->OnMouseWheelEvent(mouseWheelEvent);
    else if(mpWebView)
        mpWebView->onScroll(mouseWheelEvent);

	NOTIFY_PROCESS_STATUS(EA::WebKit::kVProcessTypeMouseWheelEvent, EA::WebKit::kVProcessStatusEnded);
}


void View::GetCursorPosition(int& x, int& y) const
{
    x = mCursorPos.x;
    y = mCursorPos.y;
}


//const EA::Raster::Point& View::GetCursorPos() const
//{
//    return mCursorPos;
//}


void View::OnFocusChangeEvent(bool /*bHasFocus*/)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	// To do: We need to find a way to tell the WebView or its ScrollView to not draw highlighting around focus objects.
}


bool View::SetModalInput(ModalInputClient* pModalInputClient)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   

	if(mpModalInputClient != pModalInputClient)
    {
        if(mpModalInputClient)
            mpModalInputClient->ModalEnd();

        mpModalInputClient = pModalInputClient;

        if(mpModalInputClient)
            mpModalInputClient->ModalBegin();
    }

	return true;
}


ModalInputClient* View::GetModalInputClient() const
{
    return mpModalInputClient;
}


void View::SetOverlaySurface(EA::Raster::Surface* pSurface, const EA::Raster::Rect& viewRect)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	// This function is not re-entrant, as it modifies its data. You cannot safely call it while within a user callback.

    OverlaySurfaceArray::iterator it;

    for(it = mOverlaySurfaceArrayContainer->mOverlaySurfaceArray.begin(); it != mOverlaySurfaceArrayContainer->mOverlaySurfaceArray.end(); ++it)
    {
        if(it->mpSurface == pSurface) // If this surface already exists and is being moved...
        {
            EA::Raster::Rect& rectCurrent = it->mViewRect;

            if(memcmp(&rectCurrent, &viewRect, sizeof(viewRect))) // If there is an in-place change...
            {
                // RedrawArea triggers a WebKit-level redraw of an area.
                // To do: It would be better if we triggered a redraw of only the area
                // exposed by the move, as the new location may overlap the old one.
                RedrawArea(rectCurrent.x, rectCurrent.y, rectCurrent.w, rectCurrent.h);
                rectCurrent = viewRect;
            }

            break;
        }
    }

    if(it == mOverlaySurfaceArrayContainer->mOverlaySurfaceArray.end()) // If it was not already present...
    {
        OverlaySurfaceInfo osi;

        osi.mpSurface = pSurface;
        osi.mViewRect = viewRect;

        mOverlaySurfaceArrayContainer->mOverlaySurfaceArray.push_back(osi);
    }

    // Blit pSurface to viewRect. 
    // We don't need to care about what might be underneath this area, 
    // but we will care about it when the overlay surface is moved or destroyed.
    if(viewRect.w && viewRect.h)
    {
        const EA::Raster::Rect sourceRect(0, 0, viewRect.w, viewRect.h);

        // 2/8/09 CSidhall - Added clip rectangle reset for overlay draw to fix partial draw bug          
        const EA::Raster::Rect savedSurfaceRect(mpSurface->mClipRect.x, mpSurface->mClipRect.y,mpSurface->mClipRect.width(),mpSurface->mClipRect.height());
        WebCore::FrameView* pFV = GetFrameView();
        if(pFV)
        {    
            WebCore::IntSize scrollOffset = pFV->scrollOffset();
            const EA::Raster::Rect visRect(pFV->contentsX() - scrollOffset.width(), pFV->contentsY() - scrollOffset.height(), pFV->width(), pFV->height());
            mpSurface->SetClipRect(&visRect);
        }

        EA::Raster::Blit(pSurface, &sourceRect, mpSurface, &viewRect, NULL);

        // 2/8/09 CSidhall Added rectangle restore (might not be needed but restores everything as it was)
        mpSurface->SetClipRect(&savedSurfaceRect);


        // Notify user of view update.
        EA::WebKit::ViewNotification* pVN = EA::WebKit::GetViewNotification();

        if(pVN)
        {
            EA::WebKit::ViewUpdateInfo vui = { this, viewRect.x, viewRect.y, viewRect.w, viewRect.h };
            pVN->ViewUpdate(vui);
        }
    }

}


void View::RemoveOverlaySurface(EA::Raster::Surface* pSurface)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	
	// This function is not re-entrant, as it modifies its data. You cannot safely call it while within a user callback.

    for(OverlaySurfaceArray::iterator it = mOverlaySurfaceArrayContainer->mOverlaySurfaceArray.begin(); it != mOverlaySurfaceArrayContainer->mOverlaySurfaceArray.end(); ++it)
    {
        if(it->mpSurface == pSurface)
        {
            EA::Raster::Rect rect = it->mViewRect;

            mOverlaySurfaceArrayContainer->mOverlaySurfaceArray.erase(it); // Must erase before triggering the redraw.

            // RedrawArea triggers a WebKit-level redraw of an area.
            // To do: It would be better if we triggered a redraw of only the area
            // exposed by the move, as the new location may overlap the old one.
            RedrawArea(rect.x, rect.y, rect.w, rect.h);

            break;
        }
    }

}


// This function (re-)blits any existing overlay surfaces over the view.
// If the underlying page is changing while an overlay surface is present
// then the drawing of the page onto the view will necessitate redrawing 
// the overlay in that area.
void View::BlitOverlaySurfaces()
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	
	for(OverlaySurfaceArray::iterator it = mOverlaySurfaceArrayContainer->mOverlaySurfaceArray.begin(); it != mOverlaySurfaceArrayContainer->mOverlaySurfaceArray.end(); ++it)
    {
        const OverlaySurfaceInfo& osi = *it;

        const EA::Raster::Rect sourceRect(0, 0, osi.mViewRect.w, osi.mViewRect.h);
        EA::Raster::Blit(osi.mpSurface, &sourceRect, mpSurface, &osi.mViewRect, NULL);
    }
}


WebView* View::GetWebView() const
{
    return mpWebView;
}


::WebFrame* View::GetWebFrame() const
{
    if(mpWebView)
    {
        ::WebFrame* pWebFrame  = mpWebView->topLevelFrame();
        EAW_ASSERT(pWebFrame);

        return pWebFrame;
    }

    return NULL;
}


WebCore::Frame* View::GetFrame() const
{
    if(mpWebView)
    {
        ::WebFrame* pWebFrame  = mpWebView->topLevelFrame();
        EAW_ASSERT(pWebFrame);

        WebCore::Frame* pFrame = pWebFrame->impl();
        EAW_ASSERT(pFrame);

        return pFrame;
    }

    return NULL;
}


WebCore::FrameView* View::GetFrameView() const
{
    if(mpWebView)
    {
        ::WebFrame* pWebFrame  = mpWebView->topLevelFrame();
        EAW_ASSERT(pWebFrame);

        WebCore::Frame* pFrame = ::core(pWebFrame);
        EAW_ASSERT(pFrame);

        WebCore::FrameView* pFrameView = pFrame->view();
        EAW_ASSERT(pFrameView);

        return pFrameView;
    }

    return NULL;
}


WebCore::Page* View::GetPage() const
{
    if(mpWebView)
    {
        WebCore::Page* pPage = mpWebView->page();
        EAW_ASSERT(pPage);

        return pPage;
    }

    return NULL;
}


WebCore::Document* View::GetDocument() const
{
    if(mpWebView)
    {
        WebCore::Frame* pFrame = GetFrame();
        EAW_ASSERT(pFrame);

        WebCore::Document* pDocument = pFrame->document();
        EAW_ASSERT(pDocument);

        return pDocument;
    }

    return NULL;
}


EA::Raster::Surface* View::GetSurface() const
{
    // We could also get this from our FrameView, but we happen to keep our
    // own pointer to this.
    return mpSurface;
}


double View::GetEstimatedProgress() const
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	if(mpWebView)
        return mpWebView->estimatedProgress(); // Returns a value in the range of [0.0, 1.0]
    return 0.0;
}

void View::ResetForNewLoad()
{
    mLoadInfo = LoadInfo();
}

class DelegateBase
{
public:
	DelegateBase(EA::WebKit::View* view) : mView(view)
	{
		// empty
	}

protected:
	bool CanJumpToNode(WebCore::Node* node)
	{
		SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
		if (node->nodeType() == WebCore::Node::ELEMENT_NODE)
		{
			WebCore::Element* element = (WebCore::Element*)node;

			if (element->isHTMLElement())
			{
				WebCore::HTMLElement* htmlElement = (WebCore::HTMLElement*)element;

				WebCore::IntRect rect = htmlElement->getRect();					
				bool isBigEnough = rect.width()>5 && rect.height() > 5;

				if (isBigEnough && (htmlElement->tagName()=="A" || htmlElement->tagName()=="INPUT" || htmlElement->tagName()=="TEXTAREA"))
				{
					if (htmlElement->computedStyle()->visibility()==WebCore::VISIBLE)
					{
						if (htmlElement->computedStyle()->display()!=WebCore::NONE)
						{
							if (!htmlElement->hasClass() || !htmlElement->classNames().contains("navigation_ignore"))
							{	
								return true;						
							}
						}
						
					}
				}
			}
		}

		return false;
	}

	const EA::WebKit::View* GetView() const { return mView; }
	EA::WebKit::View* GetView() { return mView; }


private:
	EA::WebKit::View* mView;
};

class JumpToFirstLinkDelegate : public DelegateBase
{
public:
	explicit JumpToFirstLinkDelegate(EA::WebKit::View* view)
	:	DelegateBase(view)
	,	mFoundElement(0)
	{

	}

	bool operator()(WebCore::Node* node)
	{
		SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
		if (InterestedInNode(node))
		{
			ApplyAction(node);
			// by returning false we'll taking the domwalker that we've found the node we want, and to stop.
			return false;
		}
		return true;
	}

	WebCore::Element* FoundElement() const { return mFoundElement; }


protected:
	bool InterestedInNode(WebCore::Node* node)
	{
		return CanJumpToNode(node);
	}

	void ApplyAction(WebCore::Node* node)
	{
		WebCore::Element* htmlElement = (WebCore::Element*)node;
		mFoundElement = htmlElement;
		this->GetView()->MoveMouseCursorToNode(htmlElement);
	}
private:
	WebCore::Element* mFoundElement;
};


class JumpToElementWithClassDelegate : public DelegateBase
{
public:
	explicit JumpToElementWithClassDelegate(EA::WebKit::View* view, const char* jumpToClass)
		: DelegateBase(view)
		, mFoundElement(0)
		, mJumpToClass(jumpToClass)
	{

	}

	bool operator()(WebCore::Node* node)
	{
		SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
		if (InterestedInNode(node))
		{
			ApplyAction(node);
			// by returning false we'll taking the domwalker that we've found the node we want, and to stop.
			return false;
		}
		return true;
	}

	WebCore::Element* FoundElement() const { return mFoundElement; }

protected:
	bool InterestedInNode(WebCore::Node* node)
	{

		if (node->nodeType() == WebCore::Node::ELEMENT_NODE)
		{
			WebCore::Element* element = (WebCore::Element*)node;

			if (element->isHTMLElement())
			{
				WebCore::HTMLElement* htmlElement = (WebCore::HTMLElement*)element;

				if (htmlElement->computedStyle()->visibility()==WebCore::VISIBLE)
				{
					if (htmlElement->computedStyle()->display()!=WebCore::NONE)
				    {
						if (htmlElement->hasClass() && htmlElement->classNames().contains(mJumpToClass))
						{
							    return true;						
					    }
				    }
			    }
		    }
		}

		return false;
	}

	void ApplyAction(WebCore::Node* node)
	{
		WebCore::Element* htmlElement = (WebCore::Element*)node;
		mFoundElement = htmlElement;
		this->GetView()->MoveMouseCursorToNode(htmlElement);
	}
private:
	WebCore::Element* mFoundElement;
	const char*			mJumpToClass;
};

class JumpToElementWithIdDelegate : public DelegateBase
{
public:
	explicit JumpToElementWithIdDelegate(EA::WebKit::View* view, const char* jumpToId)
		: DelegateBase(view) 
		, mFoundElement(0)
		, mJumpToId(jumpToId)
	{

	}

	bool operator()(WebCore::Node* node)
	{
		SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
		if (InterestedInNode(node))
		{
			ApplyAction(node);
			// by returning false we'll taking the domwalker that we've found the node we want, and to stop.
			return false;
		}
		return true;
	}

	WebCore::Element* FoundElement() const { return mFoundElement; }

protected:
	bool InterestedInNode(WebCore::Node* node)
	{
		if (node->nodeType() == WebCore::Node::ELEMENT_NODE)
		{
			WebCore::Element* element = (WebCore::Element*)node;

			if (element->isHTMLElement())
			{
				WebCore::HTMLElement* htmlElement = (WebCore::HTMLElement*)element;

				if (htmlElement->computedStyle()->visibility()==WebCore::VISIBLE)
				{
					if (htmlElement->id() == mJumpToId)
				    {
					    return true;
				    }
			    }
		    }
		}

		return false;
	}

	void ApplyAction(WebCore::Node* node)
	{
		WebCore::Element* htmlElement = (WebCore::Element*)node;
		mFoundElement = htmlElement;
		this->GetView()->MoveMouseCursorToNode(htmlElement);
	}
private:
	WebCore::Element*	mFoundElement;
	const char*			mJumpToId;
};

class IsNodeNavigableDelegate : DelegateBase
{
public:
	IsNodeNavigableDelegate(EA::WebKit::View* view) : DelegateBase(view), mFoundNode(false) {}

	bool operator()(WebCore::Node* node)
	{
		SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
		bool canJump = CanJumpToNode(node);
		if (canJump)
		{
			mFoundNode = true;
		}

		return !canJump;
	}

	bool FoundNode() const { return mFoundNode; }

private:
	bool mFoundNode;
};








//+ 6/11/09 Note: these following procedures were contributed by Chris Stott and the Skate Team

bool View::IsAlreadyOverNavigableElement()
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	if (mpWebView)
	{
		int x, y;

		GetCursorPosition(x, y);

		WebCore::Element* element = mpWebView->elementAtPoint(x, y);

		if (element)
		{
			IsNodeNavigableDelegate delegate(this);

			ReverseDOMWalker<IsNodeNavigableDelegate> walker(element, delegate);

			return delegate.FoundNode();
		}

	}
	
	return false;
}

//////////////////////////////////////////////////////////////////////////
//
bool View::JumpToFirstLink(const char* jumpToClass, bool skipJumpIfAlreadyOverElement)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	WebCore::Document* document = this->GetDocument();

	EAW_ASSERT(document);

	if (document)
	{
		if (!skipJumpIfAlreadyOverElement)
		{
			JumpToElementWithClassDelegate delegate1(this, jumpToClass);

			DOMWalker<JumpToElementWithClassDelegate> walker(document, delegate1);		

			if (!delegate1.FoundElement())
			{
				JumpToFirstLinkDelegate delegate2(this);

				DOMWalker<JumpToFirstLinkDelegate> walker(document, delegate2);		

				if (delegate2.FoundElement())
				{
					WebCore::Element* element = delegate2.FoundElement();
					WebCore::IntRect rect = element->getRect();
					mBestNodeX = rect.x();
					mBestNodeY = rect.y();
					mBestNodeWidth = rect.width();
					mBestNodeHeight = rect.height();
					UpdateCachedHints(element);
					return true;
				}
			}
			else
			{
				WebCore::Element* element = delegate1.FoundElement();
				WebCore::IntRect rect = element->getRect();
				mBestNodeX = rect.x();
				mBestNodeY = rect.y();
				mBestNodeWidth = rect.width();
				mBestNodeHeight = rect.height();
				UpdateCachedHints(element);
				return true;
			}

		}		
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
//
bool View::JumpToId(const char* jumpToId)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	WebCore::Document* document = this->GetDocument();

	EAW_ASSERT(document);

	if (document)
	{
		JumpToElementWithIdDelegate delegate1(this, jumpToId);

		DOMWalker<JumpToElementWithIdDelegate> walker(document, delegate1);		

		if (delegate1.FoundElement())
		{
			WebCore::Element* element = delegate1.FoundElement();
			WebCore::IntRect rect = element->getRect();
			mBestNodeX = rect.x();
			mBestNodeY = rect.y();
			mBestNodeWidth = rect.width();
			mBestNodeHeight = rect.height();
			UpdateCachedHints(element);
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void View::UpdateCachedHints(WebCore::Node* node)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	if (node && node->hasAttributes())
	{
		WebCore::NamedAttrMap* attributeMap = node->attributes();

		WebCore::Attribute* upAttribute = attributeMap->getAttributeItem("navigationup");
		WebCore::Attribute* downAttribute = attributeMap->getAttributeItem("navigationdown");
		WebCore::Attribute* leftAttribute = attributeMap->getAttributeItem("navigationleft");
		WebCore::Attribute* rightAttribute = attributeMap->getAttributeItem("navigationright");

		char buffer[100];
		wchar_t uBuffer[100];
		if ( upAttribute )
		{
			unsigned int length = upAttribute->value().length();
			wcsncpy(uBuffer, upAttribute->value().string().characters(), length);
			uBuffer[length] = L'\0';
			sprintf(buffer,"%S",uBuffer);
			GET_FIXEDSTRING8(mCachedNavigationUpId)->assign(buffer);
		}
		else
		{
			GET_FIXEDSTRING8(mCachedNavigationUpId)->assign("");
		}
		
		if ( downAttribute )
		{
			unsigned int length = downAttribute->value().length();
			wcsncpy(uBuffer, downAttribute->value().string().characters(), length);
			uBuffer[length] = L'\0';
			sprintf(buffer,"%S",uBuffer);
			GET_FIXEDSTRING8(mCachedNavigationDownId)->assign(buffer);;
	}
		else
		{
			GET_FIXEDSTRING8(mCachedNavigationDownId)->assign("");
		}

		if ( leftAttribute )
		{
			unsigned int length = leftAttribute->value().length();
			wcsncpy(uBuffer, leftAttribute->value().string().characters(), length);
			uBuffer[length] = L'\0';
			sprintf(buffer,"%S",uBuffer);
			GET_FIXEDSTRING8(mCachedNavigationLeftId)->assign(buffer);;
		}
		else
		{
			GET_FIXEDSTRING8(mCachedNavigationLeftId)->assign("");
}

		if ( rightAttribute )
		{
			unsigned int length = rightAttribute->value().length();
			wcsncpy(uBuffer, rightAttribute->value().string().characters(), length);
			uBuffer[length] = L'\0';
			sprintf(buffer,"%S",uBuffer);
			GET_FIXEDSTRING8(mCachedNavigationRightId)->assign(buffer);;
		}
		else
		{
			GET_FIXEDSTRING8(mCachedNavigationRightId)->assign("");
		}
	}
	else
	{
		GET_FIXEDSTRING8(mCachedNavigationUpId)->assign("");
		GET_FIXEDSTRING8(mCachedNavigationDownId)->assign("");
		GET_FIXEDSTRING8(mCachedNavigationLeftId)->assign("");
		GET_FIXEDSTRING8(mCachedNavigationRightId)->assign("");
	}
}

//////////////////////////////////////////////////////////////////////////
//
bool View::JumpToNearestElement(EA::WebKit::JumpDirection direction)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	if(!mpWebView)
	{
		return false;
	}

	WebCore::Document* document = this->GetDocument();
	EAW_ASSERT(document);

	int lastX		= 0;
	int lastY		= 0;
	this->GetCursorPosition( lastX, lastY );

	WebCore::IntPoint scrollOffset = this->mpWebView->scrollOffset();

	mCentreX = lastX + scrollOffset.x();
	mCentreY = lastY + scrollOffset.y();

	switch (direction)
	{
	case EA::WebKit::JumpRight:
		if (GET_FIXEDSTRING8(mCachedNavigationRightId)->compare(""))
		{
			if (!GET_FIXEDSTRING8(mCachedNavigationRightId)->compare("ignore"))
			{
				return false;
			}

			if (JumpToId(GET_FIXEDSTRING8(mCachedNavigationRightId)->c_str()))
			{
				return true;
			}
		}
		break;

	case EA::WebKit::JumpDown:
		if (GET_FIXEDSTRING8(mCachedNavigationDownId)->compare(""))
		{
			if (!GET_FIXEDSTRING8(mCachedNavigationDownId)->compare("ignore"))
			{
				return false;
			}

			if (JumpToId(GET_FIXEDSTRING8(mCachedNavigationDownId)->c_str()))
			{
				return true;
			}
		}
		break;

	case EA::WebKit::JumpLeft:
		if (GET_FIXEDSTRING8(mCachedNavigationLeftId)->compare(""))
		{
			if (!GET_FIXEDSTRING8(mCachedNavigationLeftId)->compare("ignore"))
			{
				return false;
			}

			if (JumpToId(GET_FIXEDSTRING8(mCachedNavigationLeftId)->c_str()))
			{
				return true;
			}
		}
		break;

	case EA::WebKit::JumpUp:
		if (GET_FIXEDSTRING8(mCachedNavigationUpId)->compare(""))
		{
			if (!GET_FIXEDSTRING8(mCachedNavigationUpId)->compare("ignore"))
			{
				return false;
			}

			if (JumpToId(GET_FIXEDSTRING8(mCachedNavigationUpId)->c_str()))
			{
				return true;
			}
		}
		break;

	default:
		EAW_FAIL_MSG("Should not have got here\n");
	}

	mNavigatorTheta = 1.5f;
	DocumentNavigator navigator(direction, WebCore::IntPoint(mCentreX, mCentreY), mBestNodeX, mBestNodeY, mBestNodeWidth, mBestNodeHeight, mNavigatorTheta );
	navigator.FindBestNode(document);



	MoveMouseCursorToNode(navigator.GetBestNode());
	
	bool foundSomething = false;

	if (navigator.GetBestNode())
	{
		foundSomething = true;
		WebCore::Node* bestNode = navigator.GetBestNode();
		WebCore::IntRect rect = bestNode->getRect();
		mBestNodeX = rect.x();
		mBestNodeY = rect.y();
		mBestNodeWidth = rect.width();
		mBestNodeHeight = rect.height();

		UpdateCachedHints(bestNode);
	}
	

	mNodeListContainer->mFoundNodes = navigator.mNodeListContainer->mFoundNodes;
	mNodeListContainer->mRejectedByAngleNodes = navigator.mNodeListContainer->mRejectedByAngleNodes;
	mNodeListContainer->mRejectedByRadiusNodes = navigator.mNodeListContainer->mRejectedByRadiusNodes;
	mNodeListContainer->mRejectedWouldBeTrappedNodes = navigator.mNodeListContainer->mRejectedWouldBeTrappedNodes;
	
	mAxesX = navigator.mAxesX;
	mAxesY = navigator.mAxesY;

	// this is fairly arbitrary
	const uint32_t axisLength = 500;

	switch (direction)
	{
	case EA::WebKit::JumpRight:
		mMinX = mCentreX + (int)(axisLength*cos(mNavigatorTheta));
		mMinY = mCentreY - (int)(axisLength*sin(mNavigatorTheta));

		mMaxX = mCentreX + (int)(axisLength*cos(mNavigatorTheta));
		mMaxY = mCentreY + (int)(axisLength*sin(mNavigatorTheta));
		break;

	case EA::WebKit::JumpDown:
		mMinX = mCentreX + (int)(axisLength*sin(mNavigatorTheta));
		mMinY = mCentreY + (int)(axisLength*cos(mNavigatorTheta));

		mMaxX = mCentreX - (int)(axisLength*sin(mNavigatorTheta));
		mMaxY = mCentreY + (int)(axisLength*cos(mNavigatorTheta));
		break;

	case EA::WebKit::JumpLeft:
		mMinX = mCentreX - (int)(axisLength*cos(mNavigatorTheta));
		mMinY = mCentreY + (int)(axisLength*sin(mNavigatorTheta));

		mMaxX = mCentreX - (int)(axisLength*cos(mNavigatorTheta));
		mMaxY = mCentreY - (int)(axisLength*sin(mNavigatorTheta));
		break;

	case EA::WebKit::JumpUp:
		mMinX = mCentreX - (int)(axisLength*sin(mNavigatorTheta));
		mMinY = mCentreY - (int)(axisLength*cos(mNavigatorTheta));

		mMaxX = mCentreX + (int)(axisLength*sin(mNavigatorTheta));
		mMaxY = mCentreY - (int)(axisLength*cos(mNavigatorTheta));

		break;

	default:
		EAW_FAIL_MSG("Should not have got here\n");
	}

	return foundSomething;


}

//////////////////////////////////////////////////////////////////////////
//
void View::DrawBestNode(DrawNodeCallback callback)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	if (mBestNodeWidth > 0 && mBestNodeHeight > 0)
	{
		callback(mBestNodeX,mBestNodeY,mBestNodeWidth,mBestNodeHeight, mCentreX, mCentreY);
	}
}

//////////////////////////////////////////////////////////////////////////
//
void View::DrawFoundNodes(DrawNodeCallback callback)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	for (WebCoreNodeListIterator i = mNodeListContainer->mFoundNodes.begin(); i != mNodeListContainer->mFoundNodes.end(); ++i)
	{
		WebCore::Node* node = *i;

		if (node && node->isHTMLElement())
		{
			WebCore::HTMLElement* element = (WebCore::HTMLElement*)node;

			WebCore::IntRect rect = element->getRect();

			callback(rect.x(),rect.y(),rect.width(),rect.height(), mCentreX, mCentreY);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//
void View::DrawRejectedByRadiusNodes(DrawNodeCallback callback)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	for (WebCoreNodeListIterator i = mNodeListContainer->mRejectedByRadiusNodes.begin(); i != mNodeListContainer->mRejectedByRadiusNodes.end(); ++i)
	{
		WebCore::Node* node = *i;

		if (node && node->isHTMLElement())
		{
			WebCore::HTMLElement* element = (WebCore::HTMLElement*)node;

			WebCore::IntRect rect = element->getRect();

			callback(rect.x(),rect.y(),rect.width(),rect.height(), mCentreX, mCentreY);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//
void View::DrawRejectedByAngleNodes(DrawNodeCallback callback)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	for (WebCoreNodeListIterator i = mNodeListContainer->mRejectedByAngleNodes.begin(); i != mNodeListContainer->mRejectedByAngleNodes.end(); ++i)
	{
		WebCore::Node* node = *i;

		if (node && node->isHTMLElement())
		{
			WebCore::HTMLElement* element = (WebCore::HTMLElement*)node;

			WebCore::IntRect rect = element->getRect();

			callback(rect.x(),rect.y(),rect.width(),rect.height(), mCentreX, mCentreY);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//
void View::DrawRejectedWouldBeTrappedNodes(DrawNodeCallback callback)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	for (WebCoreNodeListIterator::iterator i = mNodeListContainer->mRejectedWouldBeTrappedNodes.begin(); i != mNodeListContainer->mRejectedWouldBeTrappedNodes.end(); ++i)
	{
		WebCore::Node* node = *i;

		if (node && node->isHTMLElement())
		{
			WebCore::HTMLElement* element = (WebCore::HTMLElement*)node;

			WebCore::IntRect rect = element->getRect();

			callback(rect.x(),rect.y(),rect.width(),rect.height(), mCentreX, mCentreY);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//
void View::DrawSearchAxes(DrawAxesCallback callback)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	callback(mAxesX, mAxesY, mMinX, mMinY, mMaxX, mMaxY);
}

//////////////////////////////////////////////////////////////////////////
//
void View::EnterTextIntoSelectedInput(const char* text)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	const WebCore::Document* document = this->GetDocument();
	EAW_ASSERT(document);

	if (document)
	{
		// WebCore::ExceptionCode code=0;

		WebCore::Node* node = document->focusedNode();

		if (node)
		{
            //+ 8/12/09 CSidhall - Activated text input             
            // Old code:
			// node->setNodeValue(text,code);
		    
            // New code:
            // Find out if we have a text input node
            if( (node->focused()) && (node->isElementNode()) && (node->hasTagName( WebCore::HTMLNames::inputTag )) ) 
            {
                WebCore::HTMLInputElement* pInputElement = static_cast<WebCore::HTMLInputElement*> (node);

                // Get the flag
                bool textField = pInputElement->isTextField();
                if(textField)
                {
                    WebCore::String val(text); 
                    pInputElement->setValue(val);                   
                }
            }
            //-CS
        }		
    }
}

//////////////////////////////////////////////////////////////////////////
//
void View::AdvanceFocus(EA::WebKit::FocusDirection direction, const EA::WebKit::KeyboardEvent& event)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	EAW_ASSERT(GetFrame());
	EAW_ASSERT(GetFrame()->page());
	EAW_ASSERT(GetFrame()->page()->focusController());

	GetFrame()->page()->focusController()->advanceFocus((WebCore::FocusDirection)direction, 0);

	MoveMouseCursorToFocusElement();
}



//////////////////////////////////////////////////////////////////////////
//
void View::MoveMouseCursorToFocusElement()
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	WebCore::Document* document = this->GetDocument();

	EAW_ASSERT(document);

	if (document)
	{
		WebCore::Node* node = document->focusedNode();

		if (node)
		{
			WebCore::HTMLElement* element = (WebCore::HTMLElement*)node;

			WebCore::IntRect rect = element->getRect();
			
			{
				int lastX		= 0;
				int lastY		= 0;
				this->GetCursorPosition( lastX, lastY );

				int newX		= rect.x() + rect.width() /2;
				int newY		= rect.bottom() - rect.height() /2;

				EA::WebKit::MouseMoveEvent moveEvent;
				memset( &moveEvent, 0, sizeof(moveEvent) );
				moveEvent.mDX	= newX-lastX;
				moveEvent.mDY	= newY-lastY;
				moveEvent.mX	= Clamp( 0, newX, this->GetSurface()->mWidth );
				moveEvent.mY	= Clamp( 0, newY, this->GetSurface()->mHeight );
				this->OnMouseMoveEvent( moveEvent );
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//
void View::MoveMouseCursorToNode(WebCore::Node* node)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	if (mpWebView && node)
	{
		WebCore::HTMLElement* element = (WebCore::HTMLElement*)node;

		WebCore::IntRect rect = element->getRect();

		{
			int lastX		= 0;
			int lastY		= 0;
			this->GetCursorPosition( lastX, lastY );

			WebCore::IntPoint scrollOffset = this->mpWebView->scrollOffset();

			int newX		= rect.x() + rect.width()/2 - scrollOffset.x();
			int newY		= rect.bottom() - rect.height()/2 - scrollOffset.y();

			EA::WebKit::MouseMoveEvent moveEvent;
			memset( &moveEvent, 0, sizeof(moveEvent) );
			moveEvent.mDX	= newX-lastX;
			moveEvent.mDY	= newY-lastY;
			moveEvent.mX	= Clamp( 0, newX, this->GetSurface()->mWidth );
			moveEvent.mY	= Clamp( 0, newY, this->GetSurface()->mHeight );

			this->OnMouseMoveEvent( moveEvent );
		}
	}
}


void View::SetCursorPosition(int x, int y)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	{
		int lastX		= 0;
		int lastY		= 0;
		this->GetCursorPosition( lastX, lastY );

		int newX		= x;
		int newY		= y;

		EA::WebKit::MouseMoveEvent moveEvent;
		memset( &moveEvent, 0, sizeof(moveEvent) );
		moveEvent.mDX	= newX-lastX;
		moveEvent.mDY	= newY-lastY;
		moveEvent.mX	= Clamp( 0, newX, this->GetSurface()->mWidth );
		moveEvent.mY	= Clamp( 0, newY, this->GetSurface()->mHeight );
		this->OnMouseMoveEvent( moveEvent );
	}
}



//////////////////////////////////////////////////////////////////////////
//
void View::AttachEventsToInputs(KeyboardCallback callback)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	WebCore::Document* document = this->GetDocument();

	EAW_ASSERT(document);

	if (document)
	{
		InputDelegate id(callback);
		TextAreaDelegate tad(callback);

		DOMWalker<InputDelegate> walker(document, id);		
		DOMWalker<TextAreaDelegate> walker2(document, tad);		
	}
}

void View::AttachJavascriptDebugger()
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   

}




//////////////////////////////////////////////////////////////////////////
//
void View::AttachEventToElementBtId(const char* id, KeyboardCallback callback)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	WebCore::Document* document = this->GetDocument();

	EAW_ASSERT(document);
	EAW_ASSERT(id);

	if (document)
	{
		WebCore::Element* element = document->getElementById(id);

		if (element)
		{
			KeyboardEventListener* listener = new KeyboardEventListener(callback, (WebCore::HTMLElement*)element);
			element->setHTMLEventListener(WebCore::eventNames().clickEvent, WTF::adoptRef(listener) );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//
void View::SetElementText(WebCore::HTMLElement* htmlElement, const char* text)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	
	EAW_ASSERT(htmlElement);
	EAW_ASSERT(text);

	WebCore::ExceptionCode error = 0;
	htmlElement->setInnerHTML(text, error);
	
}

//////////////////////////////////////////////////////////////////////////
//
void View::SetInputElementValue(WebCore::HTMLElement* htmlElement, char16_t* text)
{	
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	
	EAW_ASSERT(htmlElement);
	EAW_ASSERT(text);

	if (htmlElement->tagName() == "INPUT")
	{
		WebCore::HTMLInputElement* inputElement = (WebCore::HTMLInputElement*)htmlElement;

		inputElement->setValue(text);
		inputElement->blur();
	}
	else if (htmlElement->tagName() == "TEXTAREA")
	{
		WebCore::HTMLTextAreaElement* inputElement = (WebCore::HTMLTextAreaElement*)htmlElement;

		inputElement->setValue(text);
		inputElement->blur();
	}

	
}

//////////////////////////////////////////////////////////////////////////
//
void View::SetElementTextById(const char* id, const char* text)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	
	WebCore::Document* document = this->GetDocument();

	EAW_ASSERT(document);
	EAW_ASSERT(id);
	EAW_ASSERT(text);

	if (document)
	{
		WebCore::Element* element = document->getElementById(id);

		if (element)
		{
			if (element->isHTMLElement())
			{
				WebCore::HTMLElement* htmlElement = (WebCore::HTMLElement*)element;

				WebCore::ExceptionCode error = 0;
				htmlElement->setInnerHTML(text, error);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//
bool View::ClickElementById(const char* id)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	
	WebCore::Document* document = this->GetDocument();

	EAW_ASSERT(id);
	EAW_ASSERT(document);

	if (document)
	{
        WebCore::Element* element = document->getElementById(id);

        if (element && element->isHTMLElement())
        {    
            WebCore::HTMLElement* htmlElement = (WebCore::HTMLElement*)element;

            htmlElement->click();
            return true;
        }
	}

    return false;
}

//////////////////////////////////////////////////////////////////////////
//

class ClickElementsByClassOrIdDelegate
{
public:
	ClickElementsByClassOrIdDelegate(const char* className, bool includeId) 
	:	mClassName(className)
	,	mReturnValue(false)
	,	mIncludeId(includeId)
	{
		// empty
	}

	bool operator()(WebCore::Node* node)
	{
		SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
		if (node->isElementNode())
		{
			WebCore::Element* element = (WebCore::Element*)node;

			if (element && element->isHTMLElement())
			{    
				WebCore::HTMLElement* htmlElement = (WebCore::HTMLElement*)element;

				// test class, and optionally the id
				if ((htmlElement->hasClass() && htmlElement->classNames().contains(mClassName)) || (mIncludeId && htmlElement->id() == mClassName))
				{
					htmlElement->click();
					mReturnValue = true;
				}
			}
		}
		return true;
	}

	bool GetReturnValue() const { return mReturnValue; }

private:
	const char* mClassName;
	bool		mReturnValue;
	bool		mIncludeId;
};

//////////////////////////////////////////////////////////////////////////
//
bool View::ClickElementsByClass(const char* className)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	
	WebCore::Document* document = this->GetDocument();

	EAW_ASSERT(className);
	EAW_ASSERT(document);

	ClickElementsByClassOrIdDelegate delegate(className, false);

	if (document)
	{
		DOMWalker<ClickElementsByClassOrIdDelegate> walker(document, delegate);	
	}

	return delegate.GetReturnValue();
}

//////////////////////////////////////////////////////////////////////////
//
bool View::ClickElementsByIdOrClass(const char* idOrClassName)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	
	WebCore::Document* document = this->GetDocument();

	EAW_ASSERT(idOrClassName);
	EAW_ASSERT(document);

	ClickElementsByClassOrIdDelegate delegate(idOrClassName, true);

	if (document)
	{

		DOMWalker<ClickElementsByClassOrIdDelegate> walker(document, delegate);	
	}

	return delegate.GetReturnValue();
}




//- Contributed by Chris Stott and team




//+ 8/12/09 CSidhall - Added to get the input text from a field
uint32_t View::GetTextFromSelectedInput(char* pTextBuffer, const uint32_t bufferLenght)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	
	const WebCore::Document* document = this->GetDocument();
	EAW_ASSERT(document);
    uint32_t size = 0;
	
    if( (document) && (pTextBuffer) && (bufferLenght) )
	{
		WebCore::Node* node = document->focusedNode();

        if( (node) && (node->focused()) && (node->isElementNode()) && (node->hasTagName( WebCore::HTMLNames::inputTag )) ) 
        {
            WebCore::HTMLInputElement* pInputElement = static_cast<WebCore::HTMLInputElement*> (node);

            // Get the flag
            bool textField = pInputElement->isTextField();
            if(textField)
            {
                WebCore::String val; 
                val = pInputElement->value();                   
              
                // Copy                
                size = val.length() + 1;    // + 1 For terminator 
                const UChar* pSource = val.String::charactersWithNullTermination();
                EA::Internal::Strlcpy(pTextBuffer, pSource, bufferLenght, size);
            }
        }
    }
    return size;    
}
//- CS



//////////////////////////////////////////////////////////////////////////
//
void View::CreateJavascriptBindings(const char* bindingObjectName)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	
	mJavascriptBindingObjectName = bindingObjectName;
	mJavascriptBindingObject = 	new JavascriptBindingObject(gpViewNotification);
	if(mJavascriptBindingObject)
        this->mpWebView->mainFrame()->addToJSWindowObject(mJavascriptBindingObjectName, mJavascriptBindingObject);
}

//////////////////////////////////////////////////////////////////////////
//
void View::RebindJavascript()
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	
	if(mJavascriptBindingObject)
        this->mpWebView->mainFrame()->addToJSWindowObject(mJavascriptBindingObjectName, mJavascriptBindingObject);
}

//////////////////////////////////////////////////////////////////////////
//
void View::RegisterJavascriptMethod(const char* name)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	
	if (mJavascriptBindingObject)
	{
		mJavascriptBindingObject->addMethod(name);
	}
	else
	{
		DebugLogInfo info;
		info.mpLogText = "No binding object created yet";
		gpViewNotification->DebugLog(info);
	}

	
}

//////////////////////////////////////////////////////////////////////////
//
void View::RegisterJavascriptProperty(const char* name)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	if (mJavascriptBindingObject)
	{
		mJavascriptBindingObject->addProperty(name);
	}
}

//////////////////////////////////////////////////////////////////////////
//
void View::UnregisterJavascriptMethod(const char* name)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	if (mJavascriptBindingObject)
	{
		mJavascriptBindingObject->removeMethod(name);
	}
}

//////////////////////////////////////////////////////////////////////////
//
void View::UnregisterJavascriptProperty(const char* name)
{
	SET_AUTOFPUPRECISION(kFPUPrecisionExtended);   
	if (mJavascriptBindingObject)
	{
		mJavascriptBindingObject->removeProperty(name);
	}
}


} // namespace WebKit

} // namespace EA



