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
// EAWebKitView.h
// By Paul Pedriana - 2008
///////////////////////////////////////////////////////////////////////////////


#ifndef EAWEBKIT_EAWEBKITVIEW_H
#define EAWEBKIT_EAWEBKITVIEW_H


#include <EAWebKit/EAWebKit.h>
#include <EAWebKit/EAWebKitConfig.h>
#include <EAWebKit/EAWebKitLinkHook.h>
#include <EAWebKit/EAWebKitForwardDeclarations.h>
#include <EAWebKit/EAWebkitSTLWrapper.h>
#include <EAWebKit/EAWebKitJavascriptValue.h>
#include <EARaster/EARaster.h>
#include <EARaster/EARasterColor.h>


namespace WebCore
{
    struct ResourceRequest;
	class HTMLElement;
	class Element;
	class Node;
}

namespace KJS { class Debugger; }

class BalObject;

namespace EA
{
    namespace WebKit
    {
		//////////////////////////////////////////////////////////////////////////
		// Callbacks
		typedef void (*DrawNodeCallback)(int x, int y, int w, int h, int centreX, int centreY);
		typedef void (*DrawAxesCallback)(int centreX, int centreY, int minX, int minY, int maxX, int maxY);
		typedef void (*KeyboardCallback)(WebCore::Element* element, const char16_t* initialText, const char16_t* helperText, int maxLength);
        ///////////////////////////////////////////////////////////////////////
        // View Notification
        ///////////////////////////////////////////////////////////////////////

        // CursorChangeInfo
        // The user is required to respond to this message if the user wants 
        // cursors to change appropriately within the view.
        // The implementation of a cursor/mouse must be handled by the application
        // and is not implemented by EAWebKit. However, the Windows platform has built-in
        // support for cursors and so EAWebKit does have default cursor handling
        // for Windows, though this handling may not always work in full-screen
        // DirectX applications.
        struct CursorChangeInfo
        {
            //View* mpView;         // WebKit doesn't provide a WebView context for cursor management. We could modify WebKit to do this, but we'd rather not.
            int     mNewCursorId;   // See enum CursorId, currently located in EAWebKitGraphics.h
        };


        // ScrollbarDrawInfo
        // Used to notify the user of the need to draw a scrollbar.
        // The user can return false to allow default drawing to occur.
        struct ScrollbarDrawInfo
        {
            enum ScrollbarPart { None, BackBtn, ThumbBtn, ForwardBtn };

            EA::Raster::Surface* mpSurface;             // 
            EA::Raster::Rect     mDirtyRect;            // The View/Surface dirty rectangle that needs to be updated.
            bool                 mIsVertical;           // Vertical vs. horizontal scroll bar.
            ScrollbarPart        mHoverPart;            // If other than None, the cursor is over this part and it is thus typically drawn in a highlighted state.
            EA::Raster::Rect     mRectFwdBtn;           // Rectangle that the forward/down button encompasses.
            EA::Raster::Rect     mRectBackBtn;          // Rectangle that the back/up button encompasses.
            EA::Raster::Rect     mRectThumb;            // Rectangle that the thumb encompasses.
            EA::Raster::Rect     mRectTrack;            // Rectangle that the scrollbar background encompasses.
        };

        // CustomFocusRingInfo
        // Used to notify the user of the need to draw a focus ring effect.
        // The user can return false to allow default drawing to occur.
        // Note that a "focus ring" is the highlight around an element (e.g. link) 
        // which indicates that it has focus in the view.
        struct FocusRingDrawInfo
        {
            EA::Raster::Surface* mpSurface;
            EA::Raster::Rect     mFocusRect;
            EA::Raster::Color    mSuggestedColor;
        };


        // ButtonDrawInfo
        // Used for push buttons, checkboxes, radio buttons, dropdown arrows.
        // Provides the means to allow the EAWebKit user to override how buttons are drawn.  
        // The user can return false to allow default drawing to occur.
        struct ButtonDrawInfo
        {
            enum ButtonType { None, Button, Checkbox, Radio, DropDown };

            EA::Raster::Surface* mpSurface;             // 
            EA::Raster::Rect     mDirtyRect;            // The View/Surface dirty rectangle that needs to be updated.
            EA::Raster::Rect     mButtonRect;           // Rectangle that the button encompasses.
            ButtonType           mButtonType;           // The type of button that needs to be drawn.
            bool                 mIsHovered;            // If the cursor is hovered over it and it is thus typically drawn in a highlighted state.
            bool                 mIsChecked;            // Applies to radio and checkbox only.
            bool                 mIsFocused;            // If it has the tab focus (not necessarily drawn the same as with hover). Typically you draw a dotted line around it or draw it in a highlighted state.
        };

        // TextFieldDrawInfo
        struct TextFieldDrawInfo
        {
            EA::Raster::Surface* mpSurface;             // 
            EA::Raster::Rect     mDirtyRect;            // The View/Surface dirty rectangle that needs to be updated.
            EA::Raster::Rect     mTextRect;             // Rectangle that the text editing area encompasses.
        };

        // PopupMenuDrawInfo
        // Used to draw an unactivated popup menu.
        // Typically this involves a text area on the left and a arrow-down button on the right.
        struct PopupMenuDrawInfo
        {
            EA::Raster::Surface* mpSurface;             // 
            EA::Raster::Rect     mDirtyRect;            // The View/Surface dirty rectangle that needs to be updated.
            EA::Raster::Rect     mMenuRect;             // Rectangle of the top line of the popup menu, which is always visible whether it's activated or not.
            bool                 mIsHovered;            // If the cursor is hovered over it and it is thus typically drawn in a highlighted state.
            bool                 mIsFocused;            // If it has the tab focus (not necessarily drawn the same as with hover). Typically you draw a dotted line around it or draw it in a highlighted state.
            bool                 mIsActivated;          // True if the menu has been clicked and a multi-line selection menu has appeared.
            int                  mActivatedCount;       // Number of items in the popped up mutli-line menu.
            int                  mActivatedLineHeight;  // Should be ~equal to mActivatedRect.height() / mActivatedCount.
            int                  mSelectedIndex;        // 0-based index of the currently selected item.
            EA::Raster::Rect     mActivatedRect;        // Rectangle of the text area if mIsActivated is true. This is the rect of the multi-line selection below mMenuRect.
        };


        // ViewUpdateInfo
        // Used to notify the user that the web view image has been updated.
        // The coordinates are x,y,w,h, with x/y referring to the upper-left corner of the view.
        struct ViewUpdateInfo
        {
            View* mpView;
            int   mX;
            int   mY;
            int   mW;
            int   mH;
        };


        // ErrorInfo
        // This is for user-level errors, which should be reported to the end user
        // graphically in some way. 
        struct ErrorInfo
        {
            View*           mpView;     // 
            int             mErrorId;   // Currently this refers to the internal errors in /WebKit/OrigynWebBrowser/Api/WebError.h. This needs to be exposed.
            const char16_t* mpURI;      // The relevant URI.
            const char16_t* mpContext1; // Some possibly related context string. Dependent on the error.
        };


        // AssertionFailureInfo
        // This is for code assertion failures, which are typically generated
        // via EAW_ASSERT. This failure is not something to show an end-user and
        // is typically enabled in debug builds only. See the documentation for
        // EAW_ASSERT for more information.
        struct AssertionFailureInfo
        {
            const char* mpFailureText;
        };


        // DebugLogInfo
        // This is for code traces, which are typically generated by EAW_TRACE or 
        // some underlying equivalent within WebKit. This trace is not something to 
        // show an end-user and is typically enabled in debug builds only. 
        // See the documentation for EAW_TRACE for more information.
        struct DebugLogInfo
        {
            const char* mpLogText;
        };


        // LinkNotificationInfo
        // This allows you to intercept the execution of a hyperlink and modify it or intercede in its execution entirely.
        struct LinkNotificationInfo
        {
            View*							mpView;                 // 
            bool							mbURIIntercepted;       // If set to true then EAWebKit does nothing more and doesn't attempt to follow the link. The application is assumed to take ownership of the event.
            EASTLFixedString16Wrapper		mOriginalURI;           // 
            EASTLFixedString16Wrapper		mModifiedURI;           // If this is set to anything but empty, then it is used instead of mOriginalURI.
            EASTLHeaderMapWrapper			mpOriginalHeaderMap;    // This will always point to a valid non-empty header map.
            EASTLHeaderMapWrapper			mpModifiedHeaderMap;    // This will always point to a valid but intially empty header map. If this is set to anything but empty, then it is used intead of mpOriginalHeaderMap.
        };

        // LoadInfo
        enum LoadEventType
        {
            kLETNone,
            kLETPageRequest,            // new URL request
            kLETPageRedirect,           // new URL for the original due to a redirect. Typically you want to change the URL bar to reflect this new URL.
            kLETResponseReceived,       // The server has responded to the request.
            kLETContentLengthReceived,  // The page content length is now available.
            kLETLoadCompleted,          // The page load completed successfully.
            kLETLoadFailed,             // The page load failed.
            kLETWillClose,              // The page is about to close.
            kLETLoadStarted,            // 
            kLETTitleReceived,          // The title of the page is now available.
            kLETLoadCommitted,          // The old page will be cleared as the new page seems to be available.
            kLETLayoutCompleted,        // 
            kLETWillShow,               // The window is to be shown for the first time.
            kLETLoadProgressUpdate,     // Estimated fraction [0.0 .. 1.0] of page load completion.
        };

		enum XmlHttpRequestLoadEventType
		{
			kXHRLETAbort,
			kXHRLETError,
			kXHRLETLoad,
			kXHRLETLoadStart,
			kXHRLETProgress
		};

        struct EAWEBKIT_API LoadInfo
        {
            View*						mpView;               	// The associated View.
            LoadEventType				mLET;                 	// The LoadEventType that triggered this update.
            bool						mbStarted;            	// True if the load has started.
            bool						mbCompleted;          	// True if the load has completed.
            int64_t						mContentLength;       	// -1 means unknown.
            double						mProgressEstimation;  	// [0.0 .. 1.0]
            EASTLFixedString16Wrapper   mURI;                 	// The URI associated with this current load event.
            EASTLFixedString16Wrapper   mPageTitle;           	// This gets set by the documentLoader during the load.
            uint64_t					mLastChangedTime;     	// Time (in milliseconds since View creation) of last update of this struct.
			int							mStatusCode;			// HTTP Status Code
            LoadInfo();
        };


        // FileChooserInfo
        // This is used to tell the application to display a file load or save dialog box.
        struct FileChooserInfo
        {
            //View*  mpView;            // WebKit doesn't provide a WebView context for cursor management. We could modify WebKit to do this, but we'd rather not.
            bool     mbIsLoad;          // If true, the dialog is for loading an existing file on disk. If false it is for saving to a new or existing file on disk.
            char16_t mFilePath[256];    // The user writes to mFilePath and sets mbSucces to true or false.
            bool     mbSuccess;         // The user should set this to true or false, depending on the outcome.
        };


        // StringInfo
        // This allows the user to provide localized strings to the view.
        // See WebCore::LocalizedStringType for an enumeration of all string ids.
        struct StringInfo
        {
            //View*  mpView;            // WebKit doesn't provide a WebView context for localized strings.
            int      mStringId;         // EAWebKit sets the mStringId value, and the app fills in either the 
            char8_t  mString8[256];     // mString8 or mString16 string with the localized version of that string.
            char16_t mString16[256];
        };


        // AuthenticationInfo
        // Used to request HTTP authentication (name/password) from a user.
        // This is seen in web browsers when they pop up a user/password dialog 
        // box while trying to navigate to a page. This is not the same as 
        // a user/password HTML input form seen on some pages, as this is 
        // at an HTTP level as opposed to an HTML level.
        // The ViewNotification::Authenticate function is called once with
        // mBegin = true, then it is repeatedly called with mbBegin = false
        // until the user sets mResult to a value other than zero to indicate
        // that the user has specified mName/mPassword or that the user 
        // has cancelled.
        // It is important to note that multiple authentication requests 
        // can occur at the same time, as a page may have multiple protected
        // elements that need to be loaded at the same time. The mId parameter 
        // will be different for each request globally, so you can use that 
        // to distinguish between requests. Also, each request will only have 
        // its mbBegin parameter set to true once, the first time this message 
        // is sent.
        enum AIPersist
        {
            kAIPersistNone,         // Don't save user name, password.
            kAIPersistSession,      // Save for this session (i.e. app exection).
            kAIPersistIndefinitely  // Save across sessions (i.e. app executions).
        };

        enum AIResult
        {
            kAIResultNone,          // No result yet
            kAIResultOK,            // OK, use mName/mPassword.
            kAIResultCancel         // Cancel.
        };

        struct AuthenticationInfo
        {
            View*           mpView;             // The associated View.
            uintptr_t       mId;                // Input. Unique id assigned by authentication manager for each unique authentication request. You can use this to distinguish between requests.
            uintptr_t       mUserContext;       // Input. The recevier of this message can store data here for later retrieval.
            bool            mbBegin;            // Input. True the first time Authenticate is called, false for  
            const char16_t* mpURL;              // Input.
            const char16_t* mpRealm;            // Input.
            const char16_t* mpType;             // Input. Authentication type. Usually one of Basic, Digest, or NTLM.
            char16_t        mName[64];          // Output.
            char16_t        mPassword[64];      // Output.
            AIPersist       mPersistLevel;      // Output.
            AIResult        mResult;            // Output.
        };


        // TextInputStateInfo
        // Used to indicate that the TextInput state has changed, such as when a 
        // TextInput form control is gains or loses focus activation.
        struct TextInputStateInfo
        {
            bool            mIsActivated;       // If keyboard input is active = true
            bool            mIsPasswordField;   // If a text password input field = true
            bool            mIsSearchField;     // If is a text seach field

            TextInputStateInfo();
        };


        // ClipboardEventInfo
        // Used to allow an EAWebKit view to interact with the system clipboard.
        // Return true from the ViewNotification function to indicate success.
        struct ClipboardEventInfo
        {
            bool				         mReadFromClipboard;   // If true, then this is a request to read text from the system clipboard into mText. If false then this is a request to write mText to the system clipboard. 
            EASTLFixedString16Wrapper	 mText;                // This is to be written if mReadFromClipboard is true, and read if mReadFromClipboard is false.
        };

		// Used to allow an EAWebKit view to receive notifications of XMLHttpRequest events
		struct XMLHttpRequestEventInfo
		{
			XmlHttpRequestLoadEventType		mEventType; 
			bool							mLengthComputable;
			unsigned						mLoaded;
			unsigned						mTotal;
			const char16_t*					mURI;                 // The URI associated with this current load event.
		};


        // VProcessType - Various types of processes/functions to profile
		enum VProcessType
        {
            kVProcessTypeNone = -1,                 // 
            kVProcessTypeUser1,						// Generic for internal EAWebkit debugging
            kVProcessTypeUser2,						// Generic for internal EAWebkit debugging
            kVProcessTypeUser3,						// Generic for internal EAWebkit debugging
            
            kVProcessTypeViewTick,						// Main view update tick
            kVProcessTypeTransportTick,				// Tick the network update
			kVProcessTypeKeyBoardEvent,				// Keyboard Event
			kVProcessTypeMouseMoveEvent,			// Mouse Event
			kVProcessTypeMouseButtonEvent,			// Mouse Button
			kVProcessTypeMouseWheelEvent,			// Mouse Wheel
			kVProcessTypeScrollEvent,				// Scroll Event
			kVProcessTypeScript,                    // Main JavaScript (could include other calls like canvas draw)
            kVProcessTypeDraw,                      // Main Draw (might not include some special canvas draw calls)
            kVProcessTypeTHJobs,                    // Main job loop for resource handler
            kVProcessTypeTransportJob,              // Single job loop tracking using the transport system
            kVProcessTypeFileCacheJob,              // Single job loop tracking using the file cache system
            kVProcessTypeDrawImage,                 // Single image draw (includes most decoding, resize, compression render)
            kVProcessTypeDrawImagePattern,          // Tiling image draw (includes most decoding, resize, compression render)
            kVProcessTypeDrawGlyph,                 // Font draw (includes render)
            kVProcessTypeDrawRaster,                // Low level raster draw for font and images
            kVProcessTypeImageDecoder,              // Image decoder (JPEG, GIF, PNG)
            kVProcessTypeImageCompressionPack,      // Image compression packing
            kVProcessTypeImageCompressionUnPack,    // Image compression unpacking
            kVProcessTypeJavaScriptParser,          // JavaScript parser
            kVProcessTypeJavaScriptExecute,         // JavaScript execute
            kVProcessTypeCSSParseSheet,             // CSS Sheet parse
            kVProcessTypeFontLoading,               // Font loading



			
			//****************************************************************//
				//Add any new process types above this line//
			//****************************************************************//
			kVProcessTypeLast						// Keep this at the end for array size
        };
        
        // VProcessStatus - Various states a process can go through. For most processes, it would be start and end.
		// However, jobs can have more states. To keep it less complicated, we include those states here.
		// The queued states tell exactly how long it took for the previous state to finish. However, this does not mean that all the time was spent 
		// inside the state machine. This is because a state change can cause some other code to execute and delay the exact change of state. This actually
		// works well because this is the type of behavior we are interested in investigating anyway (Which states are taking longer to change and why). 
		enum VProcessStatus
        {
            kVProcessStatusNone,
            kVProcessStatusStarted,					// Start of a process or function
            kVProcessStatusEnded,					// End of a process or function
            kVProcessStatusQueuedToInit,			// This is when the job is queued for Init state in the code.
            kVProcessStatusQueuedToConnection,		// This is when the Init state is finished and the job is queued for the Connection state.
            kVProcessStatusQueuedToTransfer,		// This is when the Connection state is finished and the job is queued for the Transfer state. 
			kVProcessStatusQueuedToDisconnect,      // This is when the Transfer state is finished and the job is queued for the Disconnect state. 
			kVProcessStatusQueuedToShutdown,		// This is when the Disconnect state is finished and the job is queued for the Shutdown state. 
			kVProcessStatusQueuedToRemove			// This is when the Shutdown state is finished and the job is queued for the Remove state. 
         };

		// ViewProcessInfo
		// This is mostly to give user insight of when certain key processes are started and stopped.
		// It can be used for profiling for example by timing the start and end status.
		// Things like URI and job information are kept in this structure to make things less complicated.
		struct ViewProcessInfo
		{
            // Variables
            VProcessType						mProcessType;
            VProcessStatus						mProcessStatus;
            double								mStartTime;					// This is a user controlled workspace clock for timing
            double								mIntermediateTime;			// This is a user controlled workspace clock for timing
            const EASTLFixedString16Wrapper*	mURI;						// The URL associated with the process, if any.
            int									mSize;						// Various usage but mostly return size info
			int									mJobId;						// Job Id of this process, if any.						
			// Constructors
            ViewProcessInfo();
            ViewProcessInfo(VProcessType,VProcessStatus);
			void ResetTime();
        };

		// Note by Arpit Baldeva: Use this function to notify an event change in state of a process found in the global array that keeps track of the 
		// predefined processes. This is what you would want to use most of the times.
		void NOTIFY_PROCESS_STATUS(VProcessType processType, VProcessStatus processStatus);
		// Note by Arpit Baldeva: Use this function to notify an event change associated with jobs. Each job has a ViewProcessInfo attached with it.
		void NOTIFY_PROCESS_STATUS(ViewProcessInfo& process,  VProcessStatus processStatus);

		// Used to pass arguments and notify the game when custom registered javascript methods are called
		struct JavascriptMethodInvokedInfo
		{
			static const unsigned MAX_ARGUMENTS = 10;

			const char*						mMethodName;
			unsigned						mArgumentCount;
			JavascriptValue					mArguments[MAX_ARGUMENTS];
			JavascriptValue					mReturn;
		};

		// Used to carry the state of a custom javascript property
		struct JavascriptPropertyInfo
		{
			const char*						mPropertyName;
			JavascriptValue					mValue;		
		};

        // The user can provide an instance of this interface to the View class.
        // The user should return true if the user handled the notification, 
        // and false if not. A return value of false means that the user wants 
        // the caller to do default handling of the notification on its own.
        // To consider: some of these notifications may be View-specific and it
        // might be a good idea to make a View::SetViewNotification() for them.
        class ViewNotification
        {
        public:
                
            
            virtual ~ViewNotification() { }

            virtual bool CursorChanged   		(CursorChangeInfo&)     	{ return false; }
            virtual bool DrawScrollbar   		(ScrollbarDrawInfo&)    	{ return false; }
            virtual bool DrawFocusRing   		(FocusRingDrawInfo&)    	{ return false; }  // A focus ring is a highlight around a link or input element indicating it has focus.
            virtual bool DrawButton      		(ButtonDrawInfo&)       	{ return false; }
            virtual bool DrawTextArea    		(TextFieldDrawInfo&)    	{ return false; }
            virtual bool DrawPopupMenu   		(PopupMenuDrawInfo&)    	{ return false; }
            virtual bool ViewUpdate      		(ViewUpdateInfo&)       	{ return false; }  // This should not be called directly but should be called through View::ViewUpdated().
            virtual bool Error           		(ErrorInfo&)            	{ return false; }
            virtual bool AssertionFailure		(AssertionFailureInfo&) 	{ return false; }
            virtual bool DebugLog        		(DebugLogInfo&)         	{ return false; }
            virtual bool LinkSelected    		(LinkNotificationInfo&) 	{ return false; }
            virtual bool LoadUpdate      		(LoadInfo&)             	{ return false; }
            virtual bool ChooseFile      		(FileChooserInfo&)      	{ return false; }
            virtual bool GetString       		(StringInfo&)           	{ return false; }
            virtual bool Authenticate    		(AuthenticationInfo&)   	{ return false; }  // Called when there is a page authentication challenge by the server.
            virtual bool TextInputState  		(TextInputStateInfo&)   	{ return false; }
            virtual bool ClipboardEvent  		(ClipboardEventInfo&)   	{ return false; }
			virtual bool XMLHttpRequestEvent	(XMLHttpRequestEventInfo&)	{ return false; }
            virtual bool ViewProcessStatus      (ViewProcessInfo&)          { return false; } // To notify for start and end of certain key processes
			virtual bool JavascriptMethodInvoked	(JavascriptMethodInvokedInfo&)	{ return false; }
			virtual bool GetJavascriptProperty		(JavascriptPropertyInfo&)		{ return false; }
			virtual bool SetJavascriptProperty		(JavascriptPropertyInfo&)		{ return false; }
        };

        // This is called by the user so that the user is notified of significant
        // events during the browsing process. There can be only a single ViewNotification
        // in place, and if you want to support more than one then you should implement
        // a proxy ViewNotification which handles this.
        EAWEBKIT_API void              SetViewNotification(ViewNotification* pViewNotification);
        EAWEBKIT_API ViewNotification* GetViewNotification();



        ///////////////////////////////////////////////////////////////////////
        // View
        ///////////////////////////////////////////////////////////////////////

        // View enumeration.
        // These functions are not thread-safe with respect to lifetime management
        // of View instances. You cannot be creating and destroying Views in other
        // threads while using these functions. However, these functions will execute
        // a memory read barrier upon reading the View maintentance data structures
        // to handle the case whereby you may have manipulated Views from other threads
        // but know that there is no further manipulation occurring.

        EAWEBKIT_API int   GetViewCount();                           // Get the current number of EA::WebKit::Views.
        EAWEBKIT_API View* GetView(int index);                       // Get the nth EA::WebKit::View, in range if [0, GetViewCount).
        EAWEBKIT_API bool  IsViewValid(View* pView);                 // 
        EAWEBKIT_API View* GetView(::WebView* pWebView);             // Get an EA::WebKit::View from a WebKit WebView.
        EAWEBKIT_API View* GetView(::WebFrame* pWebFrame);           // Get an EA::WebKit::View from a WebKit WebFrame.
        EAWEBKIT_API View* GetView(WebCore::Frame* pFrame);          // Get an EA::WebKit::View from a WebCore::Frame.
        EAWEBKIT_API View* GetView(WebCore::FrameView* pFrameView);  // Get an EA::WebKit::View from a WebCore::FrameView.

		EAWEBKIT_API View* CreateView();
		EAWEBKIT_API void DestroyView(View* pView);

        // View default values
        enum ViewDefault
        {
            kViewWidthDefault  = 800,
            kViewHeightDefault = 600
        };


        // Initialization parameters for the View class.
        struct EAWEBKIT_API ViewParameters
        {
            int   mWidth;                           // Defaults to 800
            int   mHeight;                          // Defaults to 600
            bool  mbScrollingEnabled;               // Defaults to true
            bool  mbHScrollbarEnabled;              // Defaults to true
            bool  mbVScrollbarEnabled;              // Defaults to true
            bool  mbHighlightingEnabled;            // Defaults to false
            bool  mbTransparentBackground;          // Defaults to false
            bool  mbTabKeyFocusCycle;               // Defaults to true
            bool  mbRedrawScrollbarOnCursorHover;   // Defaults to false

            ViewParameters();
        };

		// Directions : JumpUp, JumpDown, JumpLeft, JumpRight
		enum JumpDirection
		{
			JumpUp=0,
			JumpDown,
			JumpLeft,
			JumpRight
		};

		// Focus directions
		enum FocusDirection
		{
			FocusDirectionForward=0,
			FocusDirectionBackward
		};

		// Defines an overlay surface (such as a popup menu) that covers 
		// our main surface.
		struct OverlaySurfaceInfo
		{
			EA::Raster::Surface* mpSurface;
			EA::Raster::Rect     mViewRect;
		};
		
        // View is a simplified interface to WebKit's WebView.
        // This class is not thread-safe, you cannot safely use it from multiple 
        // threads simultaneously. Nor can you create multiple View instances and 
        // use them from different threads. This is due to the design of WebKit 
        // itself and there are no plans by the WebKit community to change this.
        // The Qt documentation at http://doc.trolltech.com/4.4/qwebview.html describes
        // their WebView, which is similar to ours.

		class NodeListContainer;
		class OverlaySurfaceArrayContainer;
		class EAWebKitJavascriptDebugger;

		class EAWEBKIT_API View
        {
        public:
            View();
            virtual ~View();


            ///////////////////////////////
            // Setup / configuration
            ///////////////////////////////

            virtual bool InitView(const ViewParameters& vp);
            virtual void ShutdownView();

            // View size in pixels. Includes scrollbars if present
            virtual void GetSize(int& w, int& h) const;
            virtual bool SetSize(int w, int h);

            // Font defaults 
            // To set alternative font defaults, call GetWebView()->setPreferences(WebPreferences* prefs);


            ///////////////////////////////
            // URI navigation
            ///////////////////////////////

            virtual bool SetURI(const char* pURI);
            virtual bool LoadResourceRequest(const WebCore::ResourceRequest& resourceRequest);
			virtual const char16_t* GetURI();
            virtual bool SetHTML(const char* pHTML, size_t length, const char* pBaseURL = NULL);
            virtual bool SetContent(const void* pData, size_t length, const char* pMimeType, const char* pEncoding = NULL, const char* pBaseURL = NULL);
            virtual void CancelLoad();
            virtual bool GoBack();
            virtual bool GoForward();
			virtual void Refresh();


            ///////////////////////////////
            // Misc
            ///////////////////////////////

            virtual LoadInfo&						GetLoadInfo();
            virtual EA::WebKit::JavascriptValue		EvaluateJavaScript(const char* pScriptSource, size_t length);
            virtual TextInputStateInfo&				GetTextInputStateInfo();            
            virtual void							GetCursorPosition(int& x, int& y) const; // Access the current cursor position (mouse, pointer, etc)

			virtual void							AttachJavascriptDebugger();


            ///////////////////////////////
            // Runtime
            ///////////////////////////////

            // Call this function repeatedly.
            // Returns true if the surface was changed.
            virtual bool Tick();

            // This is called by our WebKit-level code whenever an area of the
            // view has been redrawn. It does any internal housekeeping and then
            // calls the user-installed ViewNotification. View updates should go
            // through this function instead of directly calling the user-installed
            // ViewNotification callback.
            // Users of EAWebKit shouldn't normally need to use this function unless  
            // manually manipulating the draw Surface.
            virtual void ViewUpdated(int x = 0, int y = 0, int w = 0, int h = 0);

            // Triggers a forced HTML-level redraw of an area of the view.
            // Users of EAWebKit shouldn't normally need to use this function unless  
            // manually manipulating the draw Surface.
            virtual void RedrawArea(int x = 0, int y = 0, int w = 0, int h = 0);

            // Scrolls the view by a given x/y delta in pixels.
            virtual void Scroll(int x, int y);
			virtual void GetScrollOffset(int& x, int& y);


            ///////////////////////////////
            // Input events
            ///////////////////////////////

            // These functions are called by the application to notify the View
            // of input events that have occurred.
            // The x and y positions are relative to View origin, which is at the 
            // top-left corner and goes rightward and downward.
            virtual void OnKeyboardEvent(const KeyboardEvent& keyboardEvent);
            virtual void OnMouseMoveEvent(const MouseMoveEvent& mouseMoveEvent);
            virtual void OnMouseButtonEvent(const MouseButtonEvent& mouseButtonEvent);
            virtual void OnMouseWheelEvent(const MouseWheelEvent& mouseWheelEvent);
            virtual void OnFocusChangeEvent(bool bHasFocus);

            // This allows the user to implement modal view input processing, whereby 
            // input is directed to the ModalInputClient instead of to WebKit. This is useful
            // for implementing modal dialogs and popups within the view. Alternatively,
            // an application can handle this entirely externally. 
            virtual bool SetModalInput(ModalInputClient* pModalInputClient);
            virtual ModalInputClient* GetModalInputClient() const;


            ///////////////////////////////
            // Overlay Surfaces
            ///////////////////////////////

            // Add or move an overlay surface.
            // An overlay surface is a surface that is drawn on top of the WebKit View and 
            // can be moved around. This allows for the implementation of overlay windows 
            // on top of the main View Surface.
            virtual void SetOverlaySurface(EA::Raster::Surface* pSurface, const EA::Raster::Rect& viewRect);

            // Remove an existing overlay surface.
            virtual void RemoveOverlaySurface(EA::Raster::Surface* pSurface);


            ///////////////////////////////
            // WebKit Accessors
            ///////////////////////////////

            // Get the underlying WebKit WebView object.
            // The WebView is the highest level container for a web browser window;
            // It is the container of the doc/view and corresponds to a single web
            // browser view, such as a tab in FireFox. Multiple WebViews correspond
            // to multiple tabs, each with its own URL and page history.
            // From WebView you can get the WebView's Frame, FrameView, Page, etc.
            virtual ::WebView* GetWebView() const;

            // Get the View's WebFrame.
            // The WebFrame is the doc portion of the doc/view model, but it seems
            // to be a higher level wrapper for the Frame class, which itself is
            // a doc model. It remains to be understood why there are separate
            // WebFrame and Frame classes instead of a single class.
            virtual ::WebFrame* GetWebFrame() const;

            // Get the View's Frame.
            // The Frame is the the lower level implementation of the doc part of 
            // the doc/view model.
            virtual WebCore::Frame* GetFrame() const;

            // Get the View's FrameView.
            // The FrameView is the 'view' part of the doc/view model.
            virtual WebCore::FrameView* GetFrameView() const;

            // Get the View's top level Page.
            // The page represents a lower level of the Frame's data.
            // It's not clear why Page is separated from Frame in the class
            // hierarchy.
            virtual WebCore::Page* GetPage() const;

            // Get the View's top level Document.
            // The document represents the HTML content of a web page.
            virtual WebCore::Document* GetDocument() const;

            // Get the View's surface.
            // The surface is the actual bits of the frame view.
            virtual EA::Raster::Surface* GetSurface() const;

            // Returns estimated load progress as a value in the range of [0.0, 1.0]
            virtual double GetEstimatedProgress() const;

            // Accessor for view parameters
            virtual const ViewParameters& GetParameters() { return mViewParameters; }

            // Accessor for LinkHookManager.
            virtual LinkHookManager& GetLinkHookManager() { return mLinkHookManager; }

			// For quickly moving between tabable elements with DPAD (NESW or standard tab index navigation)
			virtual bool JumpToNearestElement(EA::WebKit::JumpDirection direction);
			virtual void AdvanceFocus(EA::WebKit::FocusDirection direction, const EA::WebKit::KeyboardEvent& event);
			virtual bool JumpToFirstLink(const char* jumpToClass, bool skipJumpIfAlreadyOverElement);
			virtual bool JumpToId(const char* jumpToId);
			virtual void UpdateCachedHints(WebCore::Node* node);

			// Enter text into box
			virtual void EnterTextIntoSelectedInput(const char* text);
            virtual uint32_t GetTextFromSelectedInput(char* pTextBuffer, const uint32_t bufferLenght);
			
			virtual void MoveMouseCursorToFocusElement();
			virtual void MoveMouseCursorToNode(WebCore::Node* node);
			virtual void SetCursorPosition(int x, int y);

			virtual void DrawFoundNodes(DrawNodeCallback callback);
			virtual void DrawBestNode(DrawNodeCallback callback);
			virtual void DrawSearchAxes(DrawAxesCallback callback);
			virtual void DrawRejectedByRadiusNodes(DrawNodeCallback callback);
			virtual void DrawRejectedByAngleNodes(DrawNodeCallback callback);
			virtual void DrawRejectedWouldBeTrappedNodes(DrawNodeCallback callback);

			virtual void AttachEventsToInputs(KeyboardCallback callback);
			virtual void AttachEventToElementBtId(const char* id, KeyboardCallback callback);
			virtual void SetElementTextById(const char* id, const char* text);
			virtual void SetElementText(WebCore::HTMLElement* htmlElement, const char* text);
			virtual void SetInputElementValue(WebCore::HTMLElement* htmlElement, char16_t* text);
			virtual bool IsAlreadyOverNavigableElement();

            virtual bool ClickElementById(const char* id);
			virtual bool ClickElementsByClass(const char* id);
			virtual bool ClickElementsByIdOrClass(const char* id);

            virtual void ResetForNewLoad();
            virtual void BlitOverlaySurfaces();
     			
			//////////////////////////////////////////////////////////////////////////
			// Javascript Binding
			//
			// - There is an EA javascript object globally available
			// - To use it you must register methods & listen to callbacks through the ViewNotification
			// - eg. (in javascript)
			//         EA.Trace('message');
			//		   EA.PlaySoundEffect('bell')'
			virtual void CreateJavascriptBindings(const char* bindingObjectName);
			virtual void RegisterJavascriptMethod(const char* name);
			virtual void RegisterJavascriptProperty(const char* name);
			virtual void UnregisterJavascriptMethod(const char* name);
			virtual void UnregisterJavascriptProperty(const char* name);
			virtual void RebindJavascript();
     		

        private:
            ::WebView*						    mpWebView;
            EA::Raster::Surface*				mpSurface;
            ViewParameters						mViewParameters;
            LoadInfo							mLoadInfo;
            EA::Raster::Point					mCursorPos;
            ModalInputClient*					mpModalInputClient;    // There can only be one at a time.
            OverlaySurfaceArrayContainer*	    mOverlaySurfaceArrayContainer;
            LinkHookManager						mLinkHookManager;
            TextInputStateInfo					mTextInputStateInfo;   // For tracking if text edit mode is on or off
            KJS::Debugger*						mDebugger;

			NodeListContainer*					mNodeListContainer;
			
			int									mBestNodeX;
			int									mBestNodeY;
			int									mBestNodeWidth;
			int									mBestNodeHeight;
			int									mCentreX;
			int									mCentreY;
			int									mAxesX;
			int									mAxesY;
			int									mMinX;
			int									mMinY;
			int									mMaxX;
			int									mMaxY;

			EASTLFixedString8Wrapper			mCachedNavigationUpId;
			EASTLFixedString8Wrapper			mCachedNavigationDownId;
			EASTLFixedString8Wrapper			mCachedNavigationLeftId;
			EASTLFixedString8Wrapper			mCachedNavigationRightId;

			float								mNavigatorTheta;
			EASTLFixedString16Wrapper			mURI;
			BalObject*							mJavascriptBindingObject;
			const char*							mJavascriptBindingObjectName;

        };

    } // namespace WebKit

} // namespace EA

namespace EA
{
	namespace WebKit
	{
		inline ViewParameters::ViewParameters()
			: mWidth(kViewWidthDefault),
			mHeight(kViewHeightDefault),
			mbScrollingEnabled(true),
			mbHScrollbarEnabled(true),
			mbVScrollbarEnabled(true),
			mbHighlightingEnabled(false),
			mbTransparentBackground(false),
			mbTabKeyFocusCycle(true),
			mbRedrawScrollbarOnCursorHover(false)
		{
		}
	}
}


#endif // Header include guard
