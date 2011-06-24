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
// EAWebKit.h
// By Paul Pedriana - 2008
///////////////////////////////////////////////////////////////////////////////


#ifndef EAWEBKIT_EAWEBKIT_H
#define EAWEBKIT_EAWEBKIT_H

#include <EAWebKit/EAWebKitConfig.h>
#include <EAWebKit/EAWebKitForwardDeclarations.h>
#include <EARaster/EARaster.h>
#include <EARaster/EARasterColor.h>
#include <EAWebKit/EAWebkitAllocator.h>
#include <EAWebKit/EAWebkitSTLWrapper.h>
#include <EAWebKit/EAWebKitLocalizedStringsEnum.h>
#include <EAWebKit/EAWebKitTextInterface.h>


namespace EA
{
    namespace WebKit
    {
        class TransportInfo;
		struct PlatformSocketAPI;

		///////////////////////////////////////////////////////////////////////
        // Global config
        ///////////////////////////////////////////////////////////////////////

        enum WebKitStatus
        {
            kWebKitStatusInactive,
            kWebKitStatusInitializing,
            kWebKitStatusActive,
            kWebKitStatusShuttingDown
        };

        // Init must be called before any other functions are called.
		// The application may pass an allocator instance when calling Init. If NULL is passed, a default allocator would be created and used. 
        EAWEBKIT_API bool Init(Allocator* pAllocator);
		
		//Shutdown must be called after any other functions are called.
		EAWEBKIT_API void Shutdown();
        
		EAWEBKIT_API WebKitStatus GetWebKitStatus();
        
		EAWEBKIT_API Allocator* GetAllocator();

        ///////////////////////////////////////////////////////////////////////
        // Glyph Cache
        ///////////////////////////////////////////////////////////////////////

        // The user is expected to set up the glyph cache using CreateGlyphCacheWrapperInterface()
        // This wrapper allocates memory so the user also needs to destroy it on browser exit using
        // DestroyGlyphCacheWrapperInterface()
        EAWEBKIT_API EA::Internal::IGlyphCache* GetGlyphCache();
        EAWEBKIT_API void SetGlyphCache(EA::Internal::IGlyphCache* pGlyphCache);
        EAWEBKIT_API EA::Internal::IGlyphCache* CreateGlyphCacheWrapperInterface(void* pGlyphCache);
        EAWEBKIT_API void DestroyGlyphCacheWrapperInterface(EA::Internal::IGlyphCache* pGlyphCacheInterface);

        // The user is expected to set up the font server using CreateFontServerWrapperInterface()
        // This wrapper allocates memory so the user also needs to destroy it on browser exit using
        // DestroyFontServerWrapperInterface()
        EAWEBKIT_API EA::Internal::IFontServer* GetFontServer();
		EAWEBKIT_API void SetFontServer(EA::Internal::IFontServer* pFontServer);
        EAWEBKIT_API EA::Internal::IFontServer* CreateFontServerWrapperInterface(void* pFontServer);
        EAWEBKIT_API void DestroyFontServerWrapperInterface(EA::Internal::IFontServer* pFontServerInterface);

        ///////////////////////////////////////////////////////////////////////
        // Cache
        ///////////////////////////////////////////////////////////////////////

		struct RAMCacheInfo
		{
			// These 2 values can be set by the user.
            // Having a large enough RAM cache allows for faster draw (like when scrolling)
            // and faster page reload.
            uint32_t     mRAMCacheSize;         // In bytes
			uint32_t     mPageCacheCount;       // Number of pages to cache. 
            
            //+ This is returned from GetRAMCacheUsage
			uint32_t     mRAMLiveSize;          // In bytes. Returns active or live size used.
			uint32_t     mRAMDeadSize;          // In bytes. Returns inactive or dead size used. 
            uint32_t     mRAMCacheMaxUsedSize;  // In Bytes.  Returns cache largest size used.   
            //-

			RAMCacheInfo()
				: mRAMCacheSize(4 * 1024 * 1024)
				, mPageCacheCount(1)
                , mRAMLiveSize(0)
                , mRAMDeadSize(0)
                , mRAMCacheMaxUsedSize(0)
			{
			}

			RAMCacheInfo(uint32_t ramCacheSize, uint32_t pageCacheCount)
				: mRAMCacheSize(ramCacheSize)
				, mPageCacheCount(pageCacheCount)
                , mRAMLiveSize(0)
                , mRAMDeadSize(0)
                , mRAMCacheMaxUsedSize(0)
			{

			}
		};
 
		struct DiskCacheInfo
        {
            uint32_t mDiskCacheSize;        // In bytes
            const char8_t* mCacheDiskDirectory;   // Full file path to writable directory. SetDiskCacheUsage copies this string.

			DiskCacheInfo()
				: mDiskCacheSize(256 * 1024 * 1024)
				, mCacheDiskDirectory(0)
			{
			}

			DiskCacheInfo(uint32_t diskCacheSize, const char8_t* cacheDiskDirectory)
				: mDiskCacheSize(diskCacheSize)
				, mCacheDiskDirectory(cacheDiskDirectory)
			{

			}

        };


        EAWEBKIT_API void SetRAMCacheUsage(const RAMCacheInfo& ramCacheInfo);
		EAWEBKIT_API void GetRAMCacheUsage(RAMCacheInfo& ramCacheInfo);
		EAWEBKIT_API bool SetDiskCacheUsage(const DiskCacheInfo& diskCacheInfo); //Returns a bool that Indicates if Cache directory is successfully created.
		EAWEBKIT_API void GetDiskCacheUsage(DiskCacheInfo& ramCacheInfo);
		EAWEBKIT_API void PurgeCache(bool bPurgeRAMCache, bool bPurgeFontCache, bool bPurgeDiskCache);

        ///////////////////////////////////////////////////////////////////////
        // Cookie Usage
        ///////////////////////////////////////////////////////////////////////

        struct CookieInfo
        {
            const char* mCookieFilePath;           // Full file path to a cookie file path (e.g. cache:\cookies.txt on XBox 360). SetCookieUsage copies this string; it doesn't need to be persistent.
            uint32_t    mMaxIndividualCookieSize;  // Should usually be at least 4096. The usable space is mMaxIndividualCookieSize-1.
            uint32_t    mDiskCookieStorageSize;    // Should usually be at least 32768.
            uint16_t    mMaxCookieCount;           // Max number of concurrent cookies. Should usually be at least 16. Set to zero to clear and disable cookies.
			
			CookieInfo()
				: mCookieFilePath(0)
				, mMaxIndividualCookieSize(4096)
				, mDiskCookieStorageSize(32768)
				, mMaxCookieCount(256)
			{

			}
			CookieInfo(const char* cookieFilePath, uint32_t maxIndividualCookieSize, uint32_t diskCookieStorageSize, uint16_t maxCookieCount)
				: mCookieFilePath(cookieFilePath)
				, mMaxIndividualCookieSize(maxIndividualCookieSize)
				, mDiskCookieStorageSize(diskCookieStorageSize)
				, mMaxCookieCount(maxCookieCount)
			{

			}
		};

        EAWEBKIT_API void RemoveCookies();
        EAWEBKIT_API void SetCookieUsage(const CookieInfo& cookieInfo);
        EAWEBKIT_API void GetCookieUsage(CookieInfo& cookieInfo);
        
		//An application can pass cookie text(in the standard http headers format without the Set-Cookie/Set-Cookie2 key) and the associated URL to add a 
		//cookie explicitly.  
		EAWEBKIT_API void AddCookie(const char8_t* pHeaderValue, const char8_t* pURI);
		// An application can pass in the buffer and read the cookies data.
		// The return value indicates the number of cookies actually read.
		// The buffer is expected to have enough capacity as specified using mMaxCookieCount and mMaxIndividualCookieSize in the CookieInfo structure.
		// so rawCookieData[mMaxCookieCount][mMaxIndividualCookieSize].
		EAWEBKIT_API uint16_t ReadCookies(char8_t** rawCookieData, uint16_t numCookiesToRead);

		//Normally not required but an application may want to reattach cookies in the outgoing headermap. This function would clear the current cookies from the outgoing headermap
		//and reattach the current cookies for the target URL that the cookie manager has.
		EAWEBKIT_API void ReattachCookies(TransportInfo* pTInfo);
		//Update the cookie manager with the received cookies
		EAWEBKIT_API void CookiesReceived(TransportInfo* pTInfo);

		
        ///////////////////////////////////////////////////////////////////////
        // Metrics
        ///////////////////////////////////////////////////////////////////////

        // As additional metrics are made available, they will be accessible here.

        struct NetworkMetrics
        {
            uint64_t mReadVolume;   // Number of bytes ever read.
            uint64_t mWriteVolume;  // Number of bytes ever written.

			NetworkMetrics()
				: mReadVolume(0)
				, mWriteVolume(0)
			{
			}
        };
        EAWEBKIT_API void GetNetworkMetrics(NetworkMetrics& metrics);


        ///////////////////////////////////////////////////////////////////////
        // Timer related APIs
        ///////////////////////////////////////////////////////////////////////
   
        // Returns the WebKit time as floating point seconds since 1970.
        EAWEBKIT_API double GetTime();

		//If the application using EAWebKit wants to install a high resolution timer, it can set it up by using following API.
		//By default, EAWebKit uses C time() which may not be enough for high resolution requirements.
		//EAWebKit also uses clock() at places where better resolution is required. Following API gives the application
		//explicit control over the time calculation.
		//Return value indicates floating point seconds since 1970(epoch time).
		typedef double (*EAWebKitTimerCallback)();
		EAWEBKIT_API void SetHighResolutionTimer(EAWebKitTimerCallback timer); //A resolution of at least milliseconds is expected.



        ///////////////////////////////////////////////////////////////////////
        // Notification
        ///////////////////////////////////////////////////////////////////////

        // Log channels define log/trace types that the user can enable/disable.
        // See the AssertionFailure and DebugLog notification callbacks.
        enum LogChannel
        {
            kLogChannelNotYetImplemented    = 0x00000001,
            kLogChannelFrames               = 0x00000010,
            kLogChannelLoading              = 0x00000020,
            kLogChannelPopupBlocking        = 0x00000040,
            kLogChannelEvents               = 0x00000080,
            kLogChannelEditing              = 0x00000100,
            kLogChannelTextConversion       = 0x00000200,
            kLogChannelIconDatabase         = 0x00000400,
            kLogChannelSQLDatabase          = 0x00000800,
            kLogChannelSpellingAndGrammar   = 0x00001000,
            kLogChannelBackForward          = 0x00002000,
            kLogChannelHistory              = 0x00004000,
            kLogChannelPageCache            = 0x00008000,
            kLogChannelPlatformLeaks        = 0x00010000,
            kLogChannelNetwork              = 0x00100000,
            kLogChannelFTP                  = 0x00200000,
            kLogChannelThreading            = 0x00400000,
            kLogChannelStorageAPI           = 0x00800000,
            kLogChannelMedia                = 0x01000000,
            kLogChannelPlugin               = 0x02000000,
            kLogChannelArchives             = 0x04000000
        };


		extern "C" void WTFLogEvent(const char* format, ...);

        #if EAWEBKIT_TRACE_ENABLED
            #define EAW_TRACE_MSG(msg)              WTFLogEvent("%s", msg)
            #define EAW_TRACE_FORMATTED(fmt, ...)   WTFLogEvent(fmt, __VA_ARGS__)
        #else
            #define EAW_TRACE_MSG(msg)              ((void)0)
            #define EAW_TRACE_FORMATTED(fmt, ...)   ((void)0)
        #endif



        ///////////////////////////////////////////////////////////////////////
        // Parameters 
        ///////////////////////////////////////////////////////////////////////

        enum PameterDefault
        {
            kPageTimeoutDefault     = 30,           // 
            kCheckboxSizeDefault    = 13,           // Same as FireFox.
            kSystemFontSizeDefault  = 13,           // Same as FireFox
        };

        enum ColorType
        {
            kColorActiveSelectionBack,              // Seleted text
            kColorActiveSelectionFore,
            kColorInactiveSelectionBack,
            kColorInactiveSelectionFore,
            kColorActiveListBoxSelectionBack,       // Listbox and popup list entries
            kColorActiveListBoxSelectionFore,
            kColorInactiveListBoxSelectionBack,
            kColorInactiveListBoxSelectionFore,
            kColorCount
        };

        // User setting for the fire time trigger rate (default is 30Hz)
        // Note: Using a faster rate than 30Hz could have some side effects with some 
        // javascript as might be dependent on a minimum time between ticks.
        enum FireTimerRate
        {
            kFireTimerRate120Hz,
            kFireTimerRate60Hz,
            kFireTimerRate30Hz,
            kFireTimerRate20Hz,
            kFireTimerRate15Hz
        };


        // FontDescription
        // This is a basic EAWebKit-level font description. Note that WebKit and EAText 
        // have their own similar kind of FontDescription classes which are independent 
        // of and more elaborate than this one. 
        struct EAWEBKIT_API FontDescription
        {
            char        mFamilies[92];    // This is a semicolon-delimited list of font families, such as "Arial;Helvetica".
            uint32_t    mSize;            // Pixel size. Note that pixels are not necesarily the same thing as points. A point is 1/72 of an inch. Pixels are often 1/96 of an inch or smaller.
            bool        mBold;            // True if bold. 
            bool        mItalic;          // True if italic.

            FontDescription();
        };

        struct EAWEBKIT_API Parameters
        {
            uint32_t            mLogChannelFlags;           // Defaults to (kLogChannelNotYetImplemented | kLogChannelPopupBlocking | kLogChannelEvents) in debug; 0 in release.
            const char*         mpLocale;                   // Defaults to NULL, which means "en-us". The SetParameters function copies this string, mpLocale doesn't need to persist.
            const char*         mpApplicationName;          // Defaults to NULL, which means "EAWebKit". The SetParameters function copies this string, mpApplicationName doesn't need to persist.
            const char*         mpUserAgent;                // Defaults to NULL, which means "Mozilla/5.0 (<os>; U; <os version>; <locale>) AppleWebKit/525.1 (KHTML, like Gecko) EAWebKit/1.0.0 <app name>". The SetParameters function copies this string, mpUserAgent doesn't need to persist. See http://www.useragentstring.com/
			uint32_t            mMaxTransportJobs;          // Defaults to 32. Specifies maximum number of concurrent transport jobs (e.g. HTTP requests).
            double              mTransportPollTimeSeconds;  // Defaults to 0.05 seconds. Specifies frequency of polling transport protocols.
            uint32_t            mPageTimeoutSeconds;        // Defaults to kPageTimeoutDefault. Page load timeout, in seconds.
            bool                mbVerifyPeers;              // Defaults to true. If true then we do SSL/TLS peer verification via security certificates. You should set this to false only in non-shipping builds.
            bool                mbDrawIntermediatePages;    // Defaults to true. If false then a page is only drawn after it has been completely loaded.
            float               mCaretBlinkSeconds;         // Defaults to 1.0. Defines the number of seconds for the duration of a text edit caret blink.
            uint32_t            mCheckboxSize;              // Defaults to kCheckboxSizeDefault. As page text becomes larger or screen resolution becomes higher, this value should be increased.
            EA::Raster::Color   mColors[kColorCount];       // Defaults to black and white.
            FontDescription     mSystemFontDescription;     // Defaults to kSystemFontSizeDefault, sans serif. This is the font that appears in form elements such as listboxes.
            char                mFontFamilyStandard[32];    // Defaults to "Times New Roman". These should be set to a font you have installed with the EAText FontServer.
            char                mFontFamilySerif[32];       // Defaults to "Times New Roman".
            char                mFontFamilySansSerif[32];   // Defaults to "Arial".
            char                mFontFamilyMonospace[32];   // Defaults to "Courier New".
            char                mFontFamilyCursive[32];     // Defaults to "Comic Sans MS".
            char                mFontFamilyFantasy[32];     // Defaults to "Comic Sans MS".
            int                 mDefaultFontSize;           // Defaults to 16.
            int                 mDefaultMonospaceFontSize;  // Defaults to 13.
            int                 mMinimumFontSize;           // Defaults to 1.
            int                 mMinimumLogicalFontSize;    // Defaults to 8.
            bool                mJavaScriptEnabled;         // Defaults to true.
            bool                mJavaScriptCanOpenWindows;  // Defaults to true.
            bool                mPluginsEnabled;            // Defaults to true.
            bool                mPrivateBrowsing;           // Defaults to false. If private browsing is enabled, WebKit will not store information about sites the user visits.
            bool                mTabsToLinks;               // Defaults to true. If tabsToLinks is enabled, the tab key moves to links in addition to form inputs and buttons. If disabled then the tab key just cycles through form inputs and buttons. Safari preference dialog: "Press Tab to highlight each item on a web page"
            bool                mFontSmoothingEnabled;      // Defaults to false. If enabled then font smoothing is always applied to fonts. This doesn't necessarly make type look better.
            int                 mHistoryItemLimit;          // Defaults to 100. Number of pages to keep in page history.
            int                 mHistoryAgeLimit;           // Defaults to 7. Number of days to keep page history.
            bool                mbEnableFileTransport;      // Defaults to true. This enables use of the file:// scheme to read file system files. Note that the user can install a file system to interpret the meaning of such file paths.
            bool                mbEnableDirtySDKTransport;  // Defaults to true. This enables DirtySDK HTTP transport. Has no effect unless DirtySDK is compiled and linked into the application. See USE(DIRTY_SDK)
            bool                mbEnableCurlTransport;      // Defaults to true. This enables Curl HTTP and FTP transport. Has no effect unless Curl is compiled and linked into the application. See USE(CURL)
            bool                mbEnableUTFTransport;       // Defaults to true. This enables UTFInternet HTTP and FTP transport. Has no effect unless UTFInternet is compiled and linked into the application. See USE(UTFINTERNET)
            bool                mbEnableImageCompression;   // Defaults to false. If enabled, this allows the cached ARGB image to be compressed using RLE or YCOCG_DXT5 compression.  

			//Note by Arpit Baldeva: mJavaScriptStackSize defaults to 128 KB. The core Webkit trunk allocates 2MB by default (well, they don't allocate but assume that the platform has on-demand commit capability) at the time of writing. This is not suitable for consoles with limited amount of memory and without on-demand commit capability.
			// The user can tweak this size and may be get around by using a smaller size that fits their need. If the size is too small, some JavaScript code may not execute. This would fire an assert in the debug builds.
			uint32_t			mJavaScriptStackSize;		
            bool                mEnableSmoothText;					// Defaults to false.  If enable, this allows software anti-alisasing on all text. 
            bool                mbEnableImageAdditiveAlphaBlending; // Defaults to false. If enabled, does additive alpha blending for images. Most other browsers seem to use additive. 
            bool				mbEnableProfiling;					//Defaults to false. 
            bool				mbEnableGammaCorrection;			//Defaults to true. When false, it speeds up png image decoding by avoiding gamma range checking.
			bool				mbEnableJavaScriptDebugOutput;		// Defaults to false. If enabled, this will print the results of console.log and any javascript errors/exceptions to TTY
            FireTimerRate       mFireTimerRate;						// Defaults to 30Hz. Unclear if some Javascript could be unstable if fired too frequently (>30Hz). 		
            Parameters();
        };

        EAWEBKIT_API void        SetParameters(const Parameters& parameters);
        EAWEBKIT_API Parameters& GetParameters();


		EAWEBKIT_API const char16_t* GetCharacters(const EASTLFixedString16Wrapper& str);
		EAWEBKIT_API void SetCharacters(const char16_t* chars, EASTLFixedString16Wrapper& str);

		EAWEBKIT_API const char8_t* GetCharacters(const EASTLFixedString8Wrapper& str);
		EAWEBKIT_API void SetCharacters(const char8_t* chars, EASTLFixedString8Wrapper& str);

        ///////////////////////////////////////////////////////////////////////
        // Version of EAWebKit 
        ///////////////////////////////////////////////////////////////////////

        EAWEBKIT_API const char* GetVersion();

		//Vector in the OS socket calls for the network handler
		EAWEBKIT_API void SetPlatformSocketAPI(EA::WebKit::PlatformSocketAPI& platformSocketAPI);
 
    } // namespace WebKit 

} // namespace EA 






// We include sub-headers here. We can't put these at the top
// of the file, due to header include order issues. 
#include <EAWebKit/EAWebKitInput.h>
#include <EAWebKit/EAWebKitGraphics.h>
#include <EAWebKit/EAWebKitFileSystem.h>
#include <EAWebKit/EAWebKitView.h>


namespace EA
{
	namespace WebKit
	{
		//It's important to have following functions "inline". Otherwise, the linker would complain about multiple symbols and runtime crashes.
		inline FontDescription::FontDescription() 
		{
			mFamilies[0] = 0;
			mSize        = 13;
			mBold        = false; 
			mItalic      = false;
		}
	}
}

namespace EA
{
	namespace WebKit
	{
		//Note by Arpit Baldeva: We can have a CreateEARasterInstance() similar to CreateEAWebKitInstance() exported in dll or have following function accessed through 
		//the EAWebKit instance. I don't really see why we would prefer one over another. One reason in favor of not exporting CreateEARasterInstance()
		//would be to keep the export definition files short. So for now, we'll access the EARasterInstance API through EAWebKit instance.
		EAWEBKIT_API EA::Raster::IEARaster* GetEARasterInstance();

	}
}

namespace EA
{
	namespace Raster
	{
		class Surface;
		
	}
	namespace WebKit
	{
		class ViewNotification;
		class TransportHandler;

		class IEAWebkit
		{
		public:
			virtual bool Init(Allocator* pAllocator) = 0;
			virtual void Shutdown() = 0;
			virtual void Destroy() = 0;
			
			virtual Allocator* GetAllocator() = 0;

            virtual EA::Internal::IGlyphCache* GetGlyphCache() = 0;
            virtual void SetGlyphCache(EA::Internal::IGlyphCache* pGlyphCache) = 0;
            virtual EA::Internal::IGlyphCache* CreateGlyphCacheWrapperInterface(void* pGlyphCache) = 0;
            virtual void DestroyGlyphCacheWrapperInterface(EA::Internal::IGlyphCache* pGlyphCacheInterface) = 0;
            
            virtual EA::Internal::IFontServer* GetFontServer() = 0;
			virtual void SetFontServer(EA::Internal::IFontServer* pFontServer) = 0;
            virtual EA::Internal::IFontServer* CreateFontServerWrapperInterface(void* pFontServer) = 0;
            virtual void DestroyFontServerWrapperInterface(EA::Internal::IFontServer* pFontServerInterface) =0;

            virtual void SetRAMCacheUsage(const RAMCacheInfo& ramCacheInfo) = 0;
			virtual void GetRAMCacheUsage(RAMCacheInfo& ramCacheInfo) = 0;
			virtual bool SetDiskCacheUsage(const DiskCacheInfo& diskCacheInfo) = 0;
			virtual void GetDiskCacheUsage(DiskCacheInfo& ramCacheInfo) = 0;
			virtual void PurgeCache(bool bPurgeRAMCache, bool bPurgeFontCache, bool bPurgeDiskCache) = 0;
			
			virtual void RemoveCookies() = 0;
			virtual void SetCookieUsage(const CookieInfo& cookieInfo) = 0;
			virtual void GetCookieUsage(CookieInfo& cookieInfo) = 0;
			virtual void AddCookie(const char8_t* pHeaderValue, const char8_t* pURI) = 0;
			virtual uint16_t ReadCookies(char8_t** rawCookieData, uint16_t numCookiesToRead) = 0;
			
			virtual void ReattachCookies(TransportInfo* pTInfo) = 0;
			virtual void CookiesReceived(TransportInfo* pTInfo) = 0;

			virtual void GetNetworkMetrics(NetworkMetrics& metrics) = 0;
			virtual double GetTime() = 0;
			virtual void SetHighResolutionTimer(EAWebKitTimerCallback timer) = 0;

			virtual void        SetParameters(const Parameters& parameters) = 0;
			virtual Parameters& GetParameters() = 0;
			
			virtual void        SetFileSystem(FileSystem* pFileSystem) = 0;
			virtual FileSystem* GetFileSystem() = 0;

			virtual void              SetViewNotification(ViewNotification* pViewNotification) = 0;
			virtual ViewNotification* GetViewNotification() = 0;

			virtual int   GetViewCount() = 0;                           
			virtual View* GetView(int index) = 0;                       
			virtual bool  IsViewValid(View* pView) = 0;                  
			virtual View* GetView(::WebView* pWebView) = 0;             
			virtual View* GetView(::WebFrame* pWebFrame) = 0;           
			virtual View* GetView(WebCore::Frame* pFrame) = 0;          
			virtual View* GetView(WebCore::FrameView* pFrameView) = 0;  
			
			virtual View* CreateView() = 0;
			virtual void DestroyView(View* pView) = 0;
			
			virtual EA::Raster::IEARaster* GetEARasterInstance() = 0;

			virtual WebKitStatus GetWebKitStatus() = 0;	

			virtual void              AddTransportHandler(TransportHandler* pTH, const char16_t* pScheme) = 0;
			virtual void              RemoveTransportHandler(TransportHandler* pTH, const char16_t* pScheme) = 0;
			virtual TransportHandler* GetTransportHandler(const char16_t* pScheme) = 0;
			
			virtual int32_t LoadSSLCertificate(const uint8_t *pCACert, int32_t iCertSize) = 0;
			virtual void ClearSSLCertificates() = 0;

			// Overwrites the entry if present, else creates it.
			virtual void SetHeaderMapValue(EASTLHeaderMapWrapper& headerMapWrapper, const char16_t* pKey, const char16_t* pValue) = 0;
			// If key found, returns the char16_t* for of the value. Otherwise, returns NULL.
			virtual const char16_t* GetHeaderMapValue(const EASTLHeaderMapWrapper& headerMapWrapper, const char16_t* pKey) = 0;
			// If key found, erase the element.
			virtual void EraseHeaderMapValue(EASTLHeaderMapWrapper& headerMapWrapper, const char16_t* pKey) = 0;
			

			// Returns the required strlen, which may be >= textSize. 
			// You need to provide a larger textCapacity if the return value >= textCapacity.
			virtual uint32_t SetTextFromHeaderMapWrapper(const EASTLHeaderMapWrapper& headerMapWrapper, char* pHeaderMapText, uint32_t textCapacity) = 0;
			// We read multi-line text like the following:
			//    aaaa : bbbb\n
			//    aaaa:    bbbb\n
			//    aaaa:bb\n
			//     bb\n
			//    aaaa : bbbb\n
			virtual bool SetHeaderMapWrapperFromText(const char8_t* pHeaderMapText, uint32_t textSize, EASTLHeaderMapWrapper& headerMapWrapper,  bool bExpectFirstCommandLine, bool bClearMap) = 0;

			virtual const char16_t* GetCharacters(const EASTLFixedString16Wrapper& str) = 0;
			virtual void SetCharacters(const char16_t* chars, EASTLFixedString16Wrapper& str) = 0;

			virtual const char8_t* GetCharacters(const EASTLFixedString8Wrapper& str) = 0;
			virtual void SetCharacters(const char8_t* chars, EASTLFixedString8Wrapper& str) = 0;

            virtual const char* GetVersion() = 0; // Get the EAWebKit version as a string.
			virtual void SetPlatformSocketAPI(EA::WebKit::PlatformSocketAPI& platformSocketAPI) = 0;

			virtual ~IEAWebkit()
			{

			}
		};
	}
}

#if defined(EA_PLATFORM_PS3)
	extern "C" EA::WebKit::IEAWebkit* CreateEAWebkitInstance(void);
#endif


namespace EA
{
	namespace WebKit
	{
		class EAWebkitConcrete : public IEAWebkit
		{
		public:
			virtual bool Init(Allocator* pAllocator);
			virtual void Shutdown();
			virtual void Destroy();

			virtual Allocator* GetAllocator();

			virtual EA::Internal::IGlyphCache* GetGlyphCache();
			virtual void SetGlyphCache(EA::Internal::IGlyphCache* pGlyphCache);
			virtual EA::Internal::IGlyphCache* CreateGlyphCacheWrapperInterface(void* pGlyphCache);
			virtual void DestroyGlyphCacheWrapperInterface(EA::Internal::IGlyphCache* pGlyphCacheInterface);

			virtual EA::Internal::IFontServer* GetFontServer();
			virtual void SetFontServer(EA::Internal::IFontServer* pFontServer);
			virtual EA::Internal::IFontServer* CreateFontServerWrapperInterface(void* pFontServer);
			virtual void DestroyFontServerWrapperInterface(EA::Internal::IFontServer* pFontServerInterface);

			virtual void SetRAMCacheUsage(const RAMCacheInfo& ramCacheInfo);
			virtual void GetRAMCacheUsage(RAMCacheInfo& ramCacheInfo);
			virtual bool SetDiskCacheUsage(const DiskCacheInfo& diskCacheInfo);
			virtual void GetDiskCacheUsage(DiskCacheInfo& ramCacheInfo);
			virtual void PurgeCache(bool bPurgeRAMCache, bool bPurgeFontCache, bool bPurgeDiskCache);

			virtual void RemoveCookies();
			virtual void SetCookieUsage(const CookieInfo& cookieInfo);
			virtual void GetCookieUsage(CookieInfo& cookieInfo);
			virtual void CookiesReceived(TransportInfo* pTInfo);

			virtual void AddCookie(const char8_t* pHeaderValue, const char8_t* pURI);
			virtual uint16_t ReadCookies(char8_t** rawCookieData, uint16_t numCookiesToRead);

			virtual void ReattachCookies(TransportInfo* pTInfo);
			
			virtual void GetNetworkMetrics(NetworkMetrics& metrics);
			virtual double GetTime();
			virtual void SetHighResolutionTimer(EAWebKitTimerCallback timer);

			virtual void        SetParameters(const Parameters& parameters);
			virtual Parameters& GetParameters();

			virtual void        SetFileSystem(FileSystem* pFileSystem);
			virtual FileSystem* GetFileSystem();

			virtual void              SetViewNotification(ViewNotification* pViewNotification);
			virtual ViewNotification* GetViewNotification();

			virtual int   GetViewCount();                           
			virtual View* GetView(int index);                       
			virtual bool  IsViewValid(View* pView);                  
			virtual View* GetView(::WebView* pWebView);             
			virtual View* GetView(::WebFrame* pWebFrame);           
			virtual View* GetView(WebCore::Frame* pFrame);          
			virtual View* GetView(WebCore::FrameView* pFrameView); 

			virtual View* CreateView();
			virtual void DestroyView(View* pView);

			virtual EA::Raster::IEARaster* GetEARasterInstance();

			virtual WebKitStatus GetWebKitStatus();
			
			virtual void              AddTransportHandler(TransportHandler* pTH, const char16_t* pScheme);
			virtual void              RemoveTransportHandler(TransportHandler* pTH, const char16_t* pScheme);
			virtual TransportHandler* GetTransportHandler(const char16_t* pScheme);

			virtual int32_t LoadSSLCertificate(const uint8_t *pCACert, int32_t iCertSize);
			virtual void ClearSSLCertificates();

			// Overwrites the entry if present, else creates it.
			virtual void SetHeaderMapValue(EASTLHeaderMapWrapper& headerMapWrapper, const char16_t* pKey, const char16_t* pValue);
			// If key found, returns the char16_t* for of the value. Otherwise, returns NULL.
			virtual const char16_t* GetHeaderMapValue(const EASTLHeaderMapWrapper& headerMapWrapper, const char16_t* pKey);
			// If key found, erase the element.
			virtual void EraseHeaderMapValue(EASTLHeaderMapWrapper& headerMapWrapper, const char16_t* pKey);


			// Returns the required strlen, which may be >= textSize. 
			// You need to provide a larger textCapacity if the return value >= textCapacity.
			virtual uint32_t SetTextFromHeaderMapWrapper(const EASTLHeaderMapWrapper& headerMapWrapper, char* pHeaderMapText, uint32_t textCapacity);
			// We read multi-line text like the following:
			//    aaaa : bbbb\n
			//    aaaa:    bbbb\n
			//    aaaa:bb\n
			//     bb\n
			//    aaaa : bbbb\n
			virtual bool SetHeaderMapWrapperFromText(const char* pHeaderMapText, uint32_t textSize, EASTLHeaderMapWrapper& headerMapWrapper, bool bExpectFirstCommandLine, bool bClearMap);

			virtual const char16_t* GetCharacters(const EASTLFixedString16Wrapper& str);
			virtual void SetCharacters(const char16_t* chars, EASTLFixedString16Wrapper& str);

			virtual const char8_t* GetCharacters(const EASTLFixedString8Wrapper& str);
			virtual void SetCharacters(const char8_t* chars, EASTLFixedString8Wrapper& str);
            virtual const char* GetVersion(); 
			
			virtual void SetPlatformSocketAPI(EA::WebKit::PlatformSocketAPI& platformSocketAPI);

			virtual ~EAWebkitConcrete()
			{

			}
		};


	}
}
#endif // Header include guard
