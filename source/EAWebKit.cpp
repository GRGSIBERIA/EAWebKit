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
// EAWebKit.cpp
// By Paul Pedriana - 2008
///////////////////////////////////////////////////////////////////////////////
 

#include <EAWebKit/EAWebKit.h>
#include <EAWebKit/EAWebKitGraphics.h>
#include <EARaster/EARaster.h>
#include <EAIO/FnEncode.h> 
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <Api/WebView.h>
#include <Api/WebPreferences.h>
#include <Cursor.h>
#include <Cache.h>
#include <PageCache.h>
#include <FontCache.h>
#include <ResourceHandleManager.h>
#include <CookieManager.h>
#include "MainThread.h"
#include "SharedTimer.h"
#include <EAWebKit/internal/EAWebKitTextWrapper.h>
#include <EAWebKit/internal/EAWebKitAssert.h>
#include <EAAssert/eaassert.h>
#include "JavascriptCore/kjs/interpreter.h"

#if USE(DIRTYSDK)

#include "protossl.h"
#include "platformsocketapi.h"
#include <EAWebKit/EAWebKitPlatformSocketAPI.h>

EA_COMPILETIME_ASSERT(sizeof(EA::WebKit::PlatformSocketAPI) == sizeof(PlatformSocketAPICallbacks));
EA_COMPILETIME_ASSERT((int)EA::WebKit::eSockErrCount == (int)eSockErrCount);


#endif
namespace WebCore{ extern double currentTime(); }


namespace EA
{

namespace WebKit
{
  
//////Time related  stuff
EAWebKitTimerCallback gTimerCallback = NULL;

// Temporary assertion while we figure out the best way to define cursor ids.
// See the definition of EA::WebKit::CursorId for a discussion of this.
//EA_COMPILETIME_ASSERT(((int)EA::WebKit::kCursorIdCount == (int)WKAL::kCursorIdCount) && ((int)EA::WebKit::kCursorIdNone == (int)WKAL::kCursorIdNone));
COMPILE_ASSERT(((int)EA::WebKit::kCursorIdCount == (int)WKAL::kCursorIdCount) && ((int)EA::WebKit::kCursorIdNone == (int)WKAL::kCursorIdNone), CursorId);


///////////////////////////////////////////////////////////////////////
// Allocator
///////////////////////////////////////////////////////////////////////

// DefaultAllocator is a basic malloc-based allocator.
// Many applications will want to use their own allocator.
// The allocator needs to be set before any Views are created.

//Note by Arpit Baldeva: Microsoft _aligned_malloc needs to be coupled with _aligned_free. We have Malloc and MallocAligned and Free but not FreeAligned. 
// To tackle the problem, I can think of 2 approaches. One is to allocate all the memory aligned internally which is what we do below. The alignment 
//is specified to be 1 byte which means no memory overhead (except if _aligned_malloc is doing something extra).
//2nd approach would be to stick a 4 byte magic number with each allocation to figure out if it was aligned or normal allocation.

struct DefaultAllocator : public Allocator
{
    void* Malloc(size_t size, int /*flags*/, const char* /*pName*/)
    {
        #if defined(_MSC_VER)
			return	_aligned_malloc(size, 1);
		#elif defined(__GNUC__)
			return malloc(size);
		#endif
    }

    void* MallocAligned(size_t size, size_t alignment, size_t offset, int /*flags*/, const char* /*pName*/)
    {
		(void) offset;
        #if defined(_MSC_VER)
            return _aligned_malloc(size, alignment);
        #elif defined(__GNUC__)
            return memalign(alignment, size);
        #endif
    }

    void Free(void* p, size_t /*size*/)
    {
		#if defined(_MSC_VER)
			_aligned_free(p);
		#elif defined(__GNUC__)
			free(p);
		#endif
    }

    void* Realloc(void* p, size_t size, int /*flags*/)
    {
		#if defined(_MSC_VER)
			return _aligned_realloc(p, size, 1);
		#elif defined(__GNUC__)
			return realloc(p, size);
		#endif
    }
};


Allocator*			gpAllocator = 0;

EAWEBKIT_API void SetAllocator(EA::WebKit::Allocator* pAllocator)
{
    gpAllocator = pAllocator;
}

EAWEBKIT_API Allocator* GetAllocator()
{
   	if(!gpAllocator)
	{
		static DefaultAllocator defaultAllocator;
		gpAllocator = &defaultAllocator; 
	}
	return gpAllocator;
}

///////////////////////////////////////////////////////////////////////
// Parameter
///////////////////////////////////////////////////////////////////////
  
// Default param constructor.
Parameters::Parameters()
		: mLogChannelFlags(kLogChannelNotYetImplemented | kLogChannelPopupBlocking | kLogChannelEvents),
		mpLocale(NULL),
		mpApplicationName(NULL),
		mpUserAgent(NULL),
		mMaxTransportJobs(32),
		mTransportPollTimeSeconds(0.05),
		mPageTimeoutSeconds(kPageTimeoutDefault),
		mbVerifyPeers(true),
		mbDrawIntermediatePages(true),
		mCaretBlinkSeconds(1.f),
		mCheckboxSize(kCheckboxSizeDefault),
		//mColors,
		mSystemFontDescription(),
		//mFontFamilyStandard,
		//mFontFamilySerif,
		//mFontFamilySansSerif,
		//mFontFamilyMonospace,
		//mFontFamilyCursive,
		//mFontFamilyFantasy,
		mDefaultFontSize(16),
		mDefaultMonospaceFontSize(13),
		mMinimumFontSize(1),
		mMinimumLogicalFontSize(8),
		mJavaScriptEnabled(true),
		mJavaScriptCanOpenWindows(true),
		mPluginsEnabled(true),
		mPrivateBrowsing(false),
		mTabsToLinks(true),
		mFontSmoothingEnabled(false),
		mHistoryItemLimit(100),
		mHistoryAgeLimit(7),
		mbEnableFileTransport(true),
		mbEnableDirtySDKTransport(true),
		mbEnableCurlTransport(true),
		mbEnableUTFTransport(true),
		mbEnableImageCompression(false),
		mJavaScriptStackSize(128 * 1024), //128 KB
        mEnableSmoothText(false), 
        mbEnableImageAdditiveAlphaBlending(false),
		mbEnableProfiling(false),
        mbEnableGammaCorrection(true),
#if defined(_DEBUG)
		mbEnableJavaScriptDebugOutput(true),
#else
		mbEnableJavaScriptDebugOutput(false),
#endif
		mFireTimerRate(kFireTimerRate30Hz)
	{
		mColors[kColorActiveSelectionBack]         .setRGB(0xff3875d7);
		mColors[kColorActiveSelectionFore]         .setRGB(0xffd4d4d4);
		mColors[kColorInactiveSelectionBack]       .setRGB(0xff3875d7);
		mColors[kColorInactiveSelectionFore]       .setRGB(0xffd4d4d4);
		mColors[kColorActiveListBoxSelectionBack]  .setRGB(0xff3875d7);
		mColors[kColorActiveListBoxSelectionFore]  .setRGB(0xffd4d4d4);
		mColors[kColorInactiveListBoxSelectionBack].setRGB(0xff3875d7);
		mColors[kColorInactiveListBoxSelectionFore].setRGB(0xffd4d4d4);
		mFontFamilyStandard[0] = mFontFamilySerif[0] = mFontFamilySansSerif[0] = 0;
		mFontFamilyMonospace[0] = mFontFamilyCursive[0] = mFontFamilyFantasy[0] = 0;
	}

///////////////////////////////////////////////////////////////////////
// Font / Text
///////////////////////////////////////////////////////////////////////

EA::Internal::IGlyphCache* gpGlyphCache = NULL;

EAWEBKIT_API EA::Internal::IGlyphCache* GetGlyphCache()
{
    EAW_ASSERT_MSG(gpGlyphCache, "Please set the text glyph cache using CreateGlyphCacheWrapperInterface()!");
    
    return gpGlyphCache;
}

EAWEBKIT_API void SetGlyphCache(EA::Internal::IGlyphCache* pGlyphCache)
{
    EAW_ASSERT(pGlyphCache);
    gpGlyphCache = pGlyphCache;
}

EAWEBKIT_API EA::Internal::IGlyphCache* CreateGlyphCacheWrapperInterface(void* pGlyphCache)
{
    EAW_ASSERT(pGlyphCache);
    EA::TextWrapper::GlyphCacheProxy* pProxy = WTF::fastNew<EA::TextWrapper::GlyphCacheProxy, void*>(pGlyphCache); 
    EAW_ASSERT(pProxy);
    SetGlyphCache(pProxy);
    return pProxy;  
}

EAWEBKIT_API void DestroyGlyphCacheWrapperInterface(EA::Internal::IGlyphCache* pGlyphCacheInterface)
{
    WTF::fastDelete<EA::Internal::IGlyphCache>(pGlyphCacheInterface);
}

EA::Internal::IFontServer* gpFontServer = NULL;

EAWEBKIT_API void SetFontServer(EA::Internal::IFontServer* pFontServer) 
{
    EAW_ASSERT(pFontServer);
    gpFontServer = pFontServer;
}

EAWEBKIT_API EA::Internal::IFontServer* GetFontServer()
{
    EAW_ASSERT_MSG(gpGlyphCache, "Please set the font server using CreateFontServerWrapperInterface()!");
    return gpFontServer;
}

EAWEBKIT_API EA::Internal::IFontServer* CreateFontServerWrapperInterface(void* pFontServer)
{
    EAW_ASSERT(pFontServer);
    EA::TextWrapper::FontServerProxy* pProxy = WTF::fastNew<EA::TextWrapper::FontServerProxy, void*>(pFontServer); 
    EAW_ASSERT(pProxy);
    SetFontServer(pProxy);
    return pProxy;
}

EAWEBKIT_API void DestroyFontServerWrapperInterface(EA::Internal::IFontServer* pFontServerInterface)
{
    WTF::fastDelete<EA::Internal::IFontServer>(pFontServerInterface);
}

///////////////////////////////////////////////////////////////////////
// Cache
///////////////////////////////////////////////////////////////////////

EAWEBKIT_API void SetRAMCacheUsage(const EA::WebKit::RAMCacheInfo& ramCacheInfo)
{
    // WebKit expects you to set up the cache via the static WebView::setCacheModel function.
    // We call setCacheModel, then ignore what it did and call the cache ourselves
    // directly. This is because if we don't call setCacheModel then WebView itself
    // will call it later. So by calling it now we prevent it from being called later
    // and overriding what we do here.
    WebView::setCacheModel(WebCacheModelDocumentBrowser);

    // 7/10/09 CSidhall - Testing to favor live cache over keeping around in memory unused dead data by
    // having the min dead cache size to zero.
    //const unsigned minDeadCapacity = (unsigned)ramCacheInfo.mRAMCacheSize / 8;
    const unsigned minDeadCapacity = 0;
    const unsigned maxDeadCapacity = (unsigned)ramCacheInfo.mRAMCacheSize / 4;

    WebCore::cache()->setCapacities(minDeadCapacity, maxDeadCapacity, (unsigned)ramCacheInfo.mRAMCacheSize);
    WebCore::pageCache()->setCapacity((unsigned)ramCacheInfo.mPageCacheCount);
}

EAWEBKIT_API bool SetDiskCacheUsage(const EA::WebKit::DiskCacheInfo& diskCacheInfo)
{
	EAW_ASSERT_MSG(false, "The disk cache usage system has bugs that can slowdown the application. Disabling it for now");
	return false;

	//Set params for the disk cache
	WebCore::ResourceHandleManager* pRHM = WebCore::ResourceHandleManager::sharedInstance();
	EAW_ASSERT(pRHM);

	if (!diskCacheInfo.mCacheDiskDirectory) //If there is no disk cache directory, simply return.
		return false;
	
	pRHM->SetCacheDirectory(diskCacheInfo.mCacheDiskDirectory);
	pRHM->SetMaxCacheSize(diskCacheInfo.mDiskCacheSize);
	//Disable the cache if the size passed is 0.
	if(diskCacheInfo.mDiskCacheSize)
		return pRHM->UseFileCache(true);
	else
		return pRHM->UseFileCache(false);
}


EAWEBKIT_API void GetRAMCacheUsage(EA::WebKit::RAMCacheInfo& ramCacheInfo)
{
    // 12/07/09 CSidhall - Activated some basic ram stats
    // RAM resource cache:
    WebCore::Cache* pCache = WebCore::cache();
    WebCore::Cache::Statistics cacheStats = pCache->getStatistics();

    ramCacheInfo.mRAMCacheSize = cacheStats.capacity;
    ramCacheInfo.mRAMLiveSize = cacheStats.liveSize;
    ramCacheInfo.mRAMDeadSize = cacheStats.deadSize;
    ramCacheInfo.mRAMCacheMaxUsedSize = cacheStats.maxUsedSize;


    // RAM page cache:
    WebCore::PageCache* pPageCache = WebCore::pageCache();
    ramCacheInfo.mPageCacheCount = pPageCache->capacity();    

    // Font cache:
    // size_t WebCore::FontCache::fontDataCount();
    // size_t WebCore::FontCache::inactiveFontDataCount();

}


EAWEBKIT_API void GetDiskCacheUsage(EA::WebKit::DiskCacheInfo& diskCacheInfo)
{
	//Note by Arpit Baldeva: I commented out memset and pRHM->GetCacheDirectory of this function. I don't understand the use of this function the way
	//it is coded at the moment. It does not give any information to the end user that he did not set at the first place.
	//Also, with disk cache directory simply being a char* now, we don't have the liberty to return a pointer to it from a temp location.
	//If we want to return a char* to the DiskCacheInfo, we would need to allocate it here or change some functions to return the strings.

	//memset(&diskCacheInfo, 0, sizeof(diskCacheInfo));

	WebCore::ResourceHandleManager* pRHM = WebCore::ResourceHandleManager::sharedInstance();
	EAW_ASSERT(pRHM);

	diskCacheInfo.mDiskCacheSize = pRHM->GetMaxCacheSize();
	//pRHM->GetCacheDirectory(diskCacheInfo.mCacheDiskDirectory);
}



EAWEBKIT_API void PurgeCache(bool bPurgeRAMCache, bool bPurgeFontCache, bool bPurgeDiskCache)
{
    if(bPurgeRAMCache)
    {
        WebCore::Cache* pCache = WebCore::cache();
        pCache->setDisabled(true);
        pCache->setDisabled(false);

        WebCore::PageCache* pPageCache = WebCore::pageCache();
        const int capacitySaved = pPageCache->capacity();
        pPageCache->setCapacity(0);
        pPageCache->setCapacity(capacitySaved);
    }

    if(bPurgeFontCache)
    {
        // From the mailing list:
        // The way to clear the WebCore font cache is to invoke FontCache::purgeInactiveFontData(). 
        // This will remove all unreferenced font data from the cache. There is an amount of font 
        // data that is never released from the cache and therefore leaks, namely font data created 
        // in FontCache::getFontDataForCharacters. The call to invalidate() should release all other 
        // font data, but also re-create and re-cache font data for all active fonts. 

        WebCore::FontCache::purgeInactiveFontData();

        // Disabled because this function is only available in more recent versions of WebKit.
        // We want to enable this when we get sync'd to the latest WebKit. We can also consider
        // syncing to just the parts needed to implement this function.
        // FontCache::invalidate();
    }

    if(bPurgeDiskCache)
    {
        WebCore::ResourceHandleManager* pRHM = WebCore::ResourceHandleManager::sharedInstance();
        EAW_ASSERT(pRHM);

        pRHM->ClearDiskCache();
    }
}

///////////////////////////////////////////////////////////////////////
// Cookies
///////////////////////////////////////////////////////////////////////


EAWEBKIT_API void RemoveCookies()
{
    WebCore::ResourceHandleManager* pRHM = WebCore::ResourceHandleManager::sharedInstance();
    EAW_ASSERT(pRHM);

    EA::WebKit::CookieManager* pCM = pRHM->GetCookieManager();   
    pCM->RemoveCookies();
}


EAWEBKIT_API void SetCookieUsage(const EA::WebKit::CookieInfo& cookieInfo)
{
    WebCore::ResourceHandleManager* pRHM = WebCore::ResourceHandleManager::sharedInstance();
    EAW_ASSERT(pRHM);

    EA::WebKit::CookieManager* pCM = pRHM->GetCookieManager();   
    CookieManagerParameters    params(cookieInfo.mCookieFilePath, cookieInfo.mMaxIndividualCookieSize, 
                                      cookieInfo.mDiskCookieStorageSize, cookieInfo.mMaxCookieCount);
    pCM->SetParametersAndInitialize(params);
}


EAWEBKIT_API void GetCookieUsage(EA::WebKit::CookieInfo& cookieInfo)
{
    WebCore::ResourceHandleManager* pRHM = WebCore::ResourceHandleManager::sharedInstance();
    EAW_ASSERT(pRHM);

    EA::WebKit::CookieManager* pCM = pRHM->GetCookieManager();   
    const CookieManagerParameters& params = pCM->GetParameters();

    cookieInfo.mCookieFilePath           = params.mCookieFilePath.c_str();
    cookieInfo.mMaxIndividualCookieSize  = params.mMaxIndividualCookieSize;
    cookieInfo.mDiskCookieStorageSize    = params.mMaxCookieFileSize;
    cookieInfo.mMaxCookieCount           = params.mMaxCookieCount;
}


///////////////////////////////////////////////////////////////////////
// Metrics
///////////////////////////////////////////////////////////////////////

EAWEBKIT_API void GetNetworkMetrics(NetworkMetrics& metrics)
{
    if(WebCore::ResourceHandleManager::m_pInstance)
    {
        metrics.mReadVolume  = WebCore::ResourceHandleManager::m_pInstance->m_readVolume;
        metrics.mWriteVolume = WebCore::ResourceHandleManager::m_pInstance->m_writeVolume;
    }
    else
    {
        metrics.mReadVolume  = 0;
        metrics.mWriteVolume = 0;
    }
}



///////////////////////////////////////////////////////////////////////
// Misc 
///////////////////////////////////////////////////////////////////////

EAWEBKIT_API double GetTime()
{
	return WebCore::currentTime();
}

EAWEBKIT_API void SetHighResolutionTimer(EAWebKitTimerCallback timer)
{
	gTimerCallback = timer;
}


///////////////////////////////////////////////////////////////////////
// Parameters 
///////////////////////////////////////////////////////////////////////


struct ParametersEx : public Parameters
{
    eastl::fixed_string<char, 16>  msLocale;
    eastl::fixed_string<char, 16>  msApplicationName;
    eastl::fixed_string<char, 160> msUserAgent;

    void operator=(const Parameters&);

    void Shutdown()
    {
        msLocale.set_capacity(0);
        msApplicationName.set_capacity(0);
        msUserAgent.set_capacity(0);
    }
};


void ParametersEx::operator=(const Parameters& p)
{
    Parameters::operator=(p);
}


static ParametersEx& sParametersEx()
{
	static ParametersEx sParametersEx;
	return sParametersEx;
}

EAWEBKIT_API void SetParameters(const Parameters& parameters)
{
    (sParametersEx()) = parameters;

    if(parameters.mpLocale)
        sParametersEx().msLocale = parameters.mpLocale;
    else
        sParametersEx().msLocale.clear();
    sParametersEx().mpLocale = sParametersEx().msLocale.c_str();

    if(parameters.mpApplicationName)
        sParametersEx().msApplicationName = parameters.mpApplicationName;
    else
        sParametersEx().msApplicationName.clear();
    sParametersEx().mpApplicationName = sParametersEx().msApplicationName.c_str();

    if(parameters.mpUserAgent)
        sParametersEx().msUserAgent = parameters.mpUserAgent;
    else
        sParametersEx().msUserAgent.clear();

	sParametersEx().msUserAgent.append_sprintf(" EAWebKit/%s",EAWEBKIT_VERSION); 
    sParametersEx().mpUserAgent = sParametersEx().msUserAgent.c_str();

    sParametersEx().mCaretBlinkSeconds = parameters.mCaretBlinkSeconds;
    sParametersEx().mCheckboxSize      = parameters.mCheckboxSize;
    memcpy(sParametersEx().mColors, parameters.mColors, sizeof(parameters.mColors));
    sParametersEx().mSystemFontDescription = parameters.mSystemFontDescription;

    // Here we call the WebPreferences functions that are connected to our Parameters.
    // WebPreferences refers more or less to preferences set by the Safari preferences
    // dialog box. EAWebKit sees preferences as encompassing more than that, so only
    // some of our Parameters are associated with WebKit's WebPreferences. 
    WebPreferences* pWebPreferences = WebPreferences::sharedStandardPreferences();
    WebCore::String sTemp;

    if(parameters.mFontFamilyStandard[0])
    {
        sTemp = parameters.mFontFamilyStandard;
        pWebPreferences->setStandardFontFamily(sTemp);
    }
    if(parameters.mFontFamilySerif[0])
    {
        sTemp = parameters.mFontFamilySerif;
        pWebPreferences->setSerifFontFamily(sTemp);
    }
    if(parameters.mFontFamilySansSerif[0])
    {
        sTemp = parameters.mFontFamilySansSerif;
        pWebPreferences->setSansSerifFontFamily(sTemp);
    }
    if(parameters.mFontFamilyMonospace[0])
    {
        sTemp = parameters.mFontFamilyMonospace;
        pWebPreferences->setFixedFontFamily(sTemp);
    }
    if(parameters.mFontFamilyCursive[0])
    {
        sTemp = parameters.mFontFamilyCursive;
        pWebPreferences->setCursiveFontFamily(sTemp);
    }
    if(parameters.mFontFamilyFantasy[0])
    {
        sTemp = parameters.mFontFamilyFantasy;
        pWebPreferences->setFantasyFontFamily(sTemp);
    }

    pWebPreferences->setDefaultFontSize(parameters.mDefaultFontSize);
    pWebPreferences->setDefaultFixedFontSize(parameters.mDefaultMonospaceFontSize);
    pWebPreferences->setMinimumFontSize(parameters.mMinimumFontSize);
    pWebPreferences->setMinimumLogicalFontSize(parameters.mMinimumLogicalFontSize);
    pWebPreferences->setJavaScriptEnabled(parameters.mJavaScriptEnabled);
    pWebPreferences->setJavaScriptCanOpenWindowsAutomatically(parameters.mJavaScriptCanOpenWindows);
    pWebPreferences->setPlugInsEnabled(parameters.mPluginsEnabled);
    pWebPreferences->setPrivateBrowsingEnabled(parameters.mPrivateBrowsing);
    pWebPreferences->setTabsToLinks(parameters.mTabsToLinks);
    pWebPreferences->setFontSmoothing(parameters.mFontSmoothingEnabled ? FontSmoothingTypeMedium : FontSmoothingTypeStandard);
    pWebPreferences->setHistoryItemLimit(parameters.mHistoryItemLimit);
    pWebPreferences->setHistoryAgeInDaysLimit(parameters.mHistoryAgeLimit);
	pWebPreferences->SetJavaScriptStackSize(parameters.mJavaScriptStackSize);

    // The following items are WebPreferences, but it turns out we set these preferences by other means.
    // pWebPreferences->setCookieStorageAcceptPolicy(WebKitCookieStorageAcceptPolicy acceptPolicy);
    // pWebPreferences->setCacheModel(WebCacheModel cacheModel);


	KJS::Interpreter::setShouldPrintExceptions(parameters.mbEnableJavaScriptDebugOutput);

    OWBAL::setFireTimerRate(parameters.mFireTimerRate);
}


EAWEBKIT_API Parameters& GetParameters()
{
    return sParametersEx();
}



///////////////////////////////////////////////////////////////////////
// EAWebKit Init / Shutdown
///////////////////////////////////////////////////////////////////////

static WebKitStatus gWebKitStatus = kWebKitStatusInactive;

static void SetWebKitStatus(const WebKitStatus status) 
{
   gWebKitStatus = status; 
}

EAWEBKIT_API WebKitStatus GetWebKitStatus()
{
    return gWebKitStatus;
}

EAWEBKIT_API bool Init(Allocator* pAllocator)
{
    EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusInactive), "Shutdown() needs to have been called or Init() was called twice");

	gpAllocator = pAllocator;
	
	SetWebKitStatus(kWebKitStatusInitializing);
    extern void SetPackageAllocators();
    SetPackageAllocators();
    EA::TextWrapper::InitFontSystem();

    // DeepSee is the macro-based trace system that the OWB people made. It is similar to EAAssert and EATrace.
    DS_INIT_DEEPSEE_FRAMEWORK();

	//Note by Arpit Baldeva: Set default parameters in case the user does not call SetParameters
	SetParameters(sParametersEx());	
    
	SetWebKitStatus(kWebKitStatusActive);
    return true;
}


EAWEBKIT_API void Shutdown()
{
    EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Init() needs to have been called or Shutdown() was called twice");
    
    SetWebKitStatus(kWebKitStatusShuttingDown);
    
    // We may want to do some kind of assertions here.
    sParametersEx().Shutdown();

    DS_INST_DUMP_CURRENT(IOcout);
    DS_CLEAN_DEEPSEE_FRAMEWORK();

    // 2/17/09 CSidhall - Split up in parts to resolve order dependent issues with the global defines (staticFinalize needs idAttr)
    WebView::unInitPart1();
    WebView::staticFinalizePart1();  
    WebView::unInitPart2();
    WebView::staticFinalizePart2();
    EA::TextWrapper::ShutDownFontSystem();
    
    SetWebKitStatus(kWebKitStatusInactive);
}

EAWEBKIT_API const char16_t* GetCharacters(const EASTLFixedString16Wrapper& str)
{
	EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

	return GET_FIXEDSTRING16(str)->c_str();
}

EAWEBKIT_API void SetCharacters(const char16_t* chars, EASTLFixedString16Wrapper& str)
{
	EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

	GET_FIXEDSTRING16(str)->assign(chars);
}

EAWEBKIT_API const char8_t* GetCharacters(const EASTLFixedString8Wrapper& str)
{
	EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

	return GET_FIXEDSTRING8(str)->c_str();
}

EAWEBKIT_API void SetCharacters(const char8_t* chars, EASTLFixedString8Wrapper& str)
{
	EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

	GET_FIXEDSTRING8(str)->assign(chars);
}
///////////////////////////////////////////////////////////////////////
// EAWebKit Build Version 
///////////////////////////////////////////////////////////////////////

EAWEBKIT_API const char* GetVersion()
{
    return gEAWebKitBuildVersion;
}

EAWEBKIT_API void SetPlatformSocketAPI(EA::WebKit::PlatformSocketAPI& platformSocketAPI)
{
	PlatformSocketAPICallbacks* socketCallbacks = GetPlatformSocketAPICallbacksInstance();
	if(socketCallbacks)
	{
		socketCallbacks->accept			=  platformSocketAPI.accept;
		socketCallbacks->bind			=  platformSocketAPI.bind;
		socketCallbacks->connect		=  platformSocketAPI.connect;
		socketCallbacks->gethostbyaddr	=  platformSocketAPI.gethostbyaddr;
		socketCallbacks->gethostbyname	=  platformSocketAPI.gethostbyname;
		socketCallbacks->dnslookup		=  platformSocketAPI.dnslookup;	
		socketCallbacks->getpeername	=  platformSocketAPI.getpeername;
		socketCallbacks->getsockopt		=  platformSocketAPI.getsockopt;
		socketCallbacks->listen			=  platformSocketAPI.listen;
		socketCallbacks->recv			=  platformSocketAPI.recv;
		socketCallbacks->recvfrom		=  platformSocketAPI.recvfrom;
		socketCallbacks->send			=  platformSocketAPI.send;
		socketCallbacks->sendto			=  platformSocketAPI.sendto;
		socketCallbacks->setsockopt		=  platformSocketAPI.setsockopt;
		socketCallbacks->shutdown		=  platformSocketAPI.shutdown;
		socketCallbacks->socket			=  platformSocketAPI.socket;
		socketCallbacks->close			=  platformSocketAPI.close;
		socketCallbacks->poll			=  platformSocketAPI.poll;
		socketCallbacks->getlasterror	=  platformSocketAPI.getlasterror;
	}
}


} // namespace WebKit

} // namespace EA



namespace EA
{
	namespace WebKit
	{
		EAWEBKIT_API EA::Raster::Surface* CreateSurface(int width, int height, EA::Raster::PixelFormatType pft)
		{
			return WTF::fastNew<EA::Raster::Surface, int, int, EA::Raster::PixelFormatType>(width, height, pft);
		}

		EAWEBKIT_API void DestroySurface(EA::Raster::Surface* pSurface) 
		{
			WTF::fastDelete<EA::Raster::Surface>(pSurface);
		}

		EAWEBKIT_API bool WritePPMFile(const char* pPath, EA::Raster::Surface* pSurface, bool bAlphaOnly)
		{
			return EA::Raster::WritePPMFile(pPath,pSurface,bAlphaOnly);
		}

		EAWEBKIT_API EA::Raster::IEARaster* GetEARasterInstance()
		{
			static EA::Raster::EARasterConcrete concreteInstance;
			return &concreteInstance;
		}
	}
}

namespace EA
{
	namespace WebKit
	{
		EAWEBKIT_API void ReattachCookies(TransportInfo* pTInfo)
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");
		
			//Remove existing cookies from the transport headers
			EA::WebKit::HeaderMap::iterator it;
			while((it = GET_HEADERMAP(pTInfo->mHeaderMapOut)->find_as(L"Cookie", EA::WebKit::str_iless())) != GET_HEADERMAP(pTInfo->mHeaderMapOut)->end())
				GET_HEADERMAP(pTInfo->mHeaderMapOut)->erase(it);
			
			//Attach new cookies
			WebCore::ResourceHandleManager* pRHM = WebCore::ResourceHandleManager::sharedInstance();
			EAW_ASSERT(pRHM);

			EA::WebKit::CookieManager* pCM = pRHM->GetCookieManager();   
			pCM->OnHeadersSend(pTInfo);
		}

		EAWEBKIT_API void CookiesReceived(TransportInfo* pTInfo)
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");
			//Attach new cookies
			WebCore::ResourceHandleManager* pRHM = WebCore::ResourceHandleManager::sharedInstance();
			EAW_ASSERT(pRHM);

			EA::WebKit::CookieManager* pCM = pRHM->GetCookieManager();   
			pCM->OnHeadersRead(pTInfo);

		}
			
		EAWEBKIT_API void AddCookie(const char8_t* pHeaderValue, const char8_t* pURI)
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");
		
			WebCore::ResourceHandleManager* pRHM = WebCore::ResourceHandleManager::sharedInstance();
			EAW_ASSERT(pRHM);

			EA::WebKit::CookieManager* pCM = pRHM->GetCookieManager();   
			pCM->ProcessCookieHeader(pHeaderValue, pURI);
		}

		EAWEBKIT_API uint16_t ReadCookies(char8_t** rawCookieData, uint16_t numCookiesToRead)
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			WebCore::ResourceHandleManager* pRHM = WebCore::ResourceHandleManager::sharedInstance();
			EAW_ASSERT(pRHM);

			EA::WebKit::CookieManager* pCM = pRHM->GetCookieManager();   
			return pCM->EnumerateCookies(rawCookieData, numCookiesToRead);
		}

	}
}


#if defined(EA_DLL)
#include <coreallocator/icoreallocator_interface.h>
#include <EAText/EAText.h>

namespace EA
{
	namespace WebKit
	{
		class CoreAllocatorWebkitImplementation : public EA::Allocator::ICoreAllocator
		{
		public:
			void* Alloc(size_t size, const char* name, unsigned int flags)
			{ 
				return EA::WebKit::GetAllocator()->Malloc(size, flags, name); 
			}
			void* Alloc(size_t size, const char* name, unsigned int flags, unsigned int alignment, unsigned int alignmentOffset = 0)
			{ 
				return EA::WebKit::GetAllocator()->MallocAligned(size, alignment, alignmentOffset, flags,name);
			}

			void  Free(void* p, size_t /*size*/ = 0)
			{ 
				EA::WebKit::GetAllocator()->Free(p,0);
			}
		};
		CoreAllocatorWebkitImplementation gCoreAllocatorWebkitImplementation;
		CoreAllocatorWebkitImplementation* gpCoreAllocatorWebkitImplementation = &gCoreAllocatorWebkitImplementation;
	}

	namespace Allocator
	{
		ICoreAllocator* ICoreAllocator::GetDefaultAllocator()
		{
			return &EA::WebKit::gCoreAllocatorWebkitImplementation;
		}
	}
}

//EASTL requires us to define following overloaded operators. Although it requires us to define only the first two array new operators,
//defining the rest seem to be safer.
void* operator new[](size_t size, const char* name, int flags, 
					 unsigned debugFlags, const char* file, int line)
{
	(void)name; (void)flags; (void)debugFlags; (void)file; (void)line;
	return EA::WebKit::GetAllocator()->Malloc(size, flags, "global new[] 1");
}

void operator delete[](void *p, const char* /*name*/, int /*flags*/, unsigned /*debugFlags*/, const char* /*file*/, int /*line*/)
{
	EA::WebKit::GetAllocator()->Free(p,0);
}

void* operator new[](size_t size, size_t alignment, size_t alignmentOffset, const char* name, 
					 int flags, unsigned debugFlags, const char* file, int line)
{
	(void)name; (void)flags; (void)debugFlags; (void)file; (void)line;
	return EA::WebKit::GetAllocator()->MallocAligned(size, alignment, alignmentOffset, flags,"global new[] aligned");
}

void operator delete[](void *p, size_t /*alignment*/, size_t /*alignmentOffset*/, const char* /*name*/, int /*flags*/, unsigned /*debugFlags*/, const char* /*file*/, int /*line*/)
{
	EA::WebKit::GetAllocator()->Free(p,0);
}

void* operator new[](size_t size)
{
	return EA::WebKit::GetAllocator()->Malloc(size, 0, "global new[] 2");
}

void operator delete[](void *p)
{
	EA::WebKit::GetAllocator()->Free(p,0);
}


//Note by Arpit Baldeva: we need to provide these functions to DirtySDK as we are statically linking to it inside dll/prx
extern "C" void* DirtyMemAlloc(int32_t iSize, int32_t /*iMemModule*/, int32_t /*iMemGroup*/)
{
	EA::WebKit::Allocator* pAllocator = EA::WebKit::GetAllocator();
	void* p = pAllocator->Malloc(static_cast<size_t>(iSize), 0, "EAWebKit/DirtyMemAlloc");
	return p;
}

extern "C" void DirtyMemFree(void* p, int32_t /*iMemModule*/, int32_t /*iMemGroup*/)
{
	EA::WebKit::Allocator* pAllocator = EA::WebKit::GetAllocator();
	return pAllocator->Free(p, 0); 
}

#endif


namespace EA
{
	namespace WebKit
	{
		bool EAWebkitConcrete::Init(Allocator* pAllocator)
		{
			return EA::WebKit::Init(pAllocator);
		}

		void EAWebkitConcrete::Shutdown()
		{
			EA::WebKit::Shutdown();
		}

		void EAWebkitConcrete::Destroy()
		{
			//Note by Arpit Baldeva: WTF::fastDelete calls EAWebkit allocator under the hood. Since the first thing we do after loading the relocatable module is to 
			//create an instance, the application does not have a chance to setup the allocator. So if we use EAWebkit's default allocator for creating the 
			//concrete instance and later the application swaps out the allocator with a its own supplied allocator, we would end up freeing the ConcreteInstance
			//from an allocator that did not create it. The solution is to either use a global new/delete operator or create a static EAWebkitConcrete instance.
			//I chose to make a static in case we decide to overrride global new/delete for the relocatable module in the future. Then, we would avoid inadvertently
			//creating the instance from one allocator and freeing it using another.

			//WTF::fastDelete<EAWebkitConcrete>(this);
		}

		Allocator* EAWebkitConcrete::GetAllocator()
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");
			
			return EA::WebKit::GetAllocator();
		}
		
		EA::Internal::IGlyphCache* EAWebkitConcrete::GetGlyphCache()
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");
			
			return EA::WebKit::GetGlyphCache();
		}

		void EAWebkitConcrete::SetGlyphCache(EA::Internal::IGlyphCache* pGlyphCache)
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");
			
			EA::WebKit::SetGlyphCache(pGlyphCache);
		}
		
		EA::Internal::IFontServer* EAWebkitConcrete::GetFontServer()
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");
			
			return EA::WebKit::GetFontServer();
		}

		EA::Internal::IGlyphCache* EAWebkitConcrete::CreateGlyphCacheWrapperInterface(void* pGlyphCache)
        {
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");
			
			return  EA::WebKit::CreateGlyphCacheWrapperInterface(pGlyphCache); 
        }

        void EAWebkitConcrete::DestroyGlyphCacheWrapperInterface(EA::Internal::IGlyphCache* pGlyphCacheInterface)
        {
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");
			
			EA::WebKit::DestroyGlyphCacheWrapperInterface(pGlyphCacheInterface);
        }

		void EAWebkitConcrete::SetFontServer(EA::Internal::IFontServer* pFontServer)
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			EA::WebKit::SetFontServer(pFontServer);
		}

        EA::Internal::IFontServer* EAWebkitConcrete::CreateFontServerWrapperInterface(void* pFontServer)
        {
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			return  EA::WebKit::CreateFontServerWrapperInterface(pFontServer); 
        }

        void EAWebkitConcrete::DestroyFontServerWrapperInterface(EA::Internal::IFontServer* pFontServerInterface)
        {
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			EA::WebKit::DestroyFontServerWrapperInterface(pFontServerInterface);
        }

		void EAWebkitConcrete::SetRAMCacheUsage(const EA::WebKit::RAMCacheInfo& ramCacheInfo)
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			EA::WebKit::SetRAMCacheUsage(ramCacheInfo);
		}

		void EAWebkitConcrete::GetRAMCacheUsage(EA::WebKit::RAMCacheInfo& ramCacheInfo)
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			EA::WebKit::GetRAMCacheUsage(ramCacheInfo);
		}

		bool EAWebkitConcrete::SetDiskCacheUsage(const EA::WebKit::DiskCacheInfo& diskCacheInfo)
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			return EA::WebKit::SetDiskCacheUsage(diskCacheInfo);
		}

		void EAWebkitConcrete::GetDiskCacheUsage(EA::WebKit::DiskCacheInfo& diskCacheInfo)
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			EA::WebKit::GetDiskCacheUsage(diskCacheInfo);
		}

		void EAWebkitConcrete::PurgeCache(bool bPurgeRAMCache, bool bPurgeFontCache, bool bPurgeDiskCache)
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			EA::WebKit::PurgeCache(bPurgeRAMCache,bPurgeFontCache,bPurgeDiskCache);
		}

		void EAWebkitConcrete::RemoveCookies()
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			EA::WebKit::RemoveCookies();
		}

		void EAWebkitConcrete::SetCookieUsage(const CookieInfo& cookieInfo)
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			EA::WebKit::SetCookieUsage(cookieInfo);
		}

		void EAWebkitConcrete::GetCookieUsage(CookieInfo& cookieInfo)
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			EA::WebKit::GetCookieUsage(cookieInfo);
		}

		void EAWebkitConcrete::GetNetworkMetrics(NetworkMetrics& metrics)
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			EA::WebKit::GetNetworkMetrics(metrics);
		}

		double EAWebkitConcrete::GetTime()
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");
			
			return EA::WebKit::GetTime();
		}

		void EAWebkitConcrete::SetHighResolutionTimer(EAWebKitTimerCallback timer)
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			EA::WebKit::SetHighResolutionTimer(timer);
		}

		void EAWebkitConcrete::SetParameters(const Parameters& parameters)
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			EA::WebKit::SetParameters(parameters);
		}

		Parameters& EAWebkitConcrete::GetParameters()
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			return EA::WebKit::GetParameters();
		}
		
		void EAWebkitConcrete::SetFileSystem(FileSystem* pFileSystem)
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			EA::WebKit::SetFileSystem(pFileSystem);
		}

		FileSystem* EAWebkitConcrete::GetFileSystem()
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			return EA::WebKit::GetFileSystem();
		}
		
		void EAWebkitConcrete::SetViewNotification(ViewNotification* pViewNotification)
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			EA::WebKit::SetViewNotification(pViewNotification);
		}

		ViewNotification* EAWebkitConcrete::GetViewNotification()
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			return EA::WebKit::GetViewNotification();
		}
		
		int EAWebkitConcrete::GetViewCount()
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			return EA::WebKit::GetViewCount();
		}

		View* EAWebkitConcrete::GetView(int index)
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			return EA::WebKit::GetView(index);
		}

		bool EAWebkitConcrete::IsViewValid(View* pView)
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			return EA::WebKit::IsViewValid(pView);
		}

		View* EAWebkitConcrete::GetView(::WebView* pWebView)
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			return EA::WebKit::GetView(pWebView);
		}
		
		View* EAWebkitConcrete::GetView(::WebFrame* pWebFrame)
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			return EA::WebKit::GetView(pWebFrame);
		}

		View* EAWebkitConcrete::GetView(WebCore::Frame* pFrame)
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			return EA::WebKit::GetView(pFrame);
		}

		View* EAWebkitConcrete::GetView(WebCore::FrameView* pFrameView)
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			return EA::WebKit::GetView(pFrameView);
		}
		
		View* EAWebkitConcrete::CreateView()
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			return EA::WebKit::CreateView();
		}

		void EAWebkitConcrete::DestroyView(View* pView)
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			EA::WebKit::DestroyView(pView);
		}

		EA::Raster::IEARaster* EAWebkitConcrete::GetEARasterInstance()
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			return EA::WebKit::GetEARasterInstance();
		}

		WebKitStatus EAWebkitConcrete::GetWebKitStatus()
        {
            return EA::WebKit::GetWebKitStatus();
        }

		void  EAWebkitConcrete::AddTransportHandler(TransportHandler* pTH, const char16_t* pScheme)
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			EA::WebKit::AddTransportHandler(pTH, pScheme);
		}
		void  EAWebkitConcrete::RemoveTransportHandler(TransportHandler* pTH, const char16_t* pScheme)
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			EA::WebKit::RemoveTransportHandler(pTH, pScheme);
		}
		TransportHandler* EAWebkitConcrete::GetTransportHandler(const char16_t* pScheme)
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			return EA::WebKit::GetTransportHandler(pScheme);
		}

		int32_t EAWebkitConcrete::LoadSSLCertificate(const uint8_t *pCACert, int32_t iCertSize)
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");
#if USE(DIRTYSDK)
			int32_t result = ProtoSSLSetCACert(pCACert, iCertSize); 
			EAW_ASSERT(result > 0);
			return result;
#endif
			//Arpit - Temp hack to compile ps3/xenon build because they are not using DirtySDK yet

			return -1;
		}

		void EAWebkitConcrete::ClearSSLCertificates()
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");
#if USE(DIRTYSDK)
			ProtoSSLClrCACerts();
#endif
		}
		void EAWebkitConcrete::SetHeaderMapValue(EASTLHeaderMapWrapper& headerMapWrapper, const char16_t* pKey, const char16_t* pValue)
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");
			
			EA::WebKit::SetHeaderMapValue(headerMapWrapper, pKey, pValue);
		}

		const char16_t* EAWebkitConcrete::GetHeaderMapValue(const EASTLHeaderMapWrapper& headerMapWrapper, const char16_t* pKey)
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			return EA::WebKit::GetHeaderMapValue(headerMapWrapper, pKey);
		}
		void EAWebkitConcrete::EraseHeaderMapValue(EASTLHeaderMapWrapper& headerMapWrapper, const char16_t* pKey)
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			EA::WebKit::EraseHeaderMapValue(headerMapWrapper, pKey);
		}

		
		uint32_t EAWebkitConcrete::SetTextFromHeaderMapWrapper(const EASTLHeaderMapWrapper& headerMapWrapper, char* pHeaderMapText, uint32_t textCapacity)
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			return EA::WebKit::SetTextFromHeaderMapWrapper(headerMapWrapper, pHeaderMapText, textCapacity);

		}
		
		bool EAWebkitConcrete::SetHeaderMapWrapperFromText(const char* pHeaderMapText, uint32_t textSize, EASTLHeaderMapWrapper& headerMapWrapper, bool bExpectFirstCommandLine, bool bClearMap)
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			return EA::WebKit::SetHeaderMapWrapperFromText(pHeaderMapText, textSize, headerMapWrapper, bExpectFirstCommandLine, bClearMap);
		}


		const char16_t* EAWebkitConcrete::GetCharacters(const EASTLFixedString16Wrapper& str) 
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			return EA::WebKit::GetCharacters(str);
		}
		
		void EAWebkitConcrete::SetCharacters(const char16_t* chars, EASTLFixedString16Wrapper& str)
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			EA::WebKit::SetCharacters(chars, str);
		}

		const char8_t* EAWebkitConcrete::GetCharacters(const EASTLFixedString8Wrapper& str)  
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			return EA::WebKit::GetCharacters(str);
		}
		
		void EAWebkitConcrete::SetCharacters(const char8_t* chars, EASTLFixedString8Wrapper& str) 
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			EA::WebKit::SetCharacters(chars, str);
		}

		void EAWebkitConcrete::ReattachCookies(TransportInfo* pTInfo)
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");
			
			EA::WebKit::ReattachCookies(pTInfo);
		}
	
		void EAWebkitConcrete::CookiesReceived(TransportInfo* pTInfo)
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			EA::WebKit::CookiesReceived(pTInfo);
		}

		void EAWebkitConcrete::AddCookie(const char8_t* pHeaderValue, const char8_t* pURI)
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			EA::WebKit::AddCookie(pHeaderValue, pURI);
		}

		uint16_t EAWebkitConcrete::ReadCookies(char8_t** rawCookieData, uint16_t numCookiesToRead)
		{
			EAW_ASSERT_MSG( (GetWebKitStatus() == kWebKitStatusActive), "Did you call EAWebKit::Init()?");

			return EA::WebKit::ReadCookies(rawCookieData, numCookiesToRead);
		}
		
        const char* EAWebkitConcrete::GetVersion()
		{
			return EA::WebKit::GetVersion();
		}

		void EAWebkitConcrete::SetPlatformSocketAPI(EA::WebKit::PlatformSocketAPI& platformSocketAPI)
		{
			EA::WebKit::SetPlatformSocketAPI(platformSocketAPI);
		}
	}
}



#if (EAWEBKIT_PS3_PRX || EAWEBKIT_XENON_DLL ||EAWEBKIT_WINDOWS_DLL)
extern "C" EA::WebKit::IEAWebkit* CreateEAWebkitInstance(void)
{
	//Note by Arpit Baldeva: WTF::fastNew calls EAWebkit allocator under the hood. Since the first thing we do after loading the relocatable module is to 
	//create an instance, the application does not have a chance to setup the allocator. So if we use EAWebkit's default allocator for creating the 
	//concrete instance and later the application swaps out the allocator with a its own supplied allocator, we would end up freeing the ConcreteInstance
	//from an allocator that did not create it. The solution is to either use a global new/delete operator or create a static EAWebkitConcrete instance.
	//I chose to make a static in case we decide to overrride global new/delete for the relocatable module in the future. Then, we would avoid inadvertently
	//creating the instance from one allocator and freeing it using another.
	
	static EA::WebKit::EAWebkitConcrete concreteInstance;
	return &concreteInstance;
}

#if defined(_WIN32) || defined(_WIN64) //Windows or Xbox 360
#include <EAWebKit/DLLInterface.h>
#elif defined(__PPU__) //PS3
#include <sys/DLLInterface.h>
#endif

extern "C" int EAWebKitDllStart(void)
{
	//Set the allocators to be used by these packages when the module is loaded.
	EA::IO::SetAllocator(EA::WebKit::gpCoreAllocatorWebkitImplementation);
	EA::Text::SetAllocator(EA::WebKit::gpCoreAllocatorWebkitImplementation);

	return PLATFORM_DLL_START_SUCCESS;
}

extern "C" int EAWebKitDllStop(void)
{
	return PLATFORM_DLL_STOP_SUCCESS;

}

PLATFORM_DLL_START(EAWebKitDllStart);
PLATFORM_DLL_STOP(EAWebKitDllStop);

#if defined(__PPU__)

PLATFORM_DLL_MODULE(EAWebkitPrx, 0, 1, 1);
PLATFORM_DLL_LIB(cellPrx_EAWebkitPrx, PLATFORM_DLL_LIB_ATTR_REGISTER | PLATFORM_DLL_LIB_ATTR_DEPENDENT_LOAD);
PLATFORM_DLL_EXPORT_FUNC(CreateEAWebkitInstance, cellPrx_EAWebkitPrx);

extern "C" int __cxa_pure_virtual ()
{
	return 0;
}

#endif

#if defined(_WIN32) || defined(_WIN64)
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4255)
#endif //_MSC_VER

#if !defined(_M_PPC)
#include <windows.h>
#endif

#if defined(_M_PPC)
#include <comdecl.h>
#endif

#if defined(_MSC_VER)
#pragma warning(pop)
#endif //_MSC_VER

BOOL APIENTRY DllMain( HANDLE hModule,
					  DWORD ul_reason_for_call,
					  LPVOID lpReserved
					  );

BOOL APIENTRY DllMain( HANDLE hModule,
					  DWORD ul_reason_for_call,
					  LPVOID lpReserved
					  )
{
	BOOL returnValue = TRUE;

	switch( ul_reason_for_call )
	{
	case DLL_PROCESS_ATTACH:
		if(platform_dll_start_func() != PLATFORM_DLL_START_SUCCESS)
			returnValue = FALSE;
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		if(platform_dll_stop_func() != PLATFORM_DLL_STOP_SUCCESS)
			returnValue = FALSE;
		break;
	}

	return returnValue;
}

#endif //defined(_WIN32) || defined(_WIN64)
#endif



