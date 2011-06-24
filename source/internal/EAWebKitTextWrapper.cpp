/*
Copyright (C) 2009 Electronic Arts, Inc.  All rights reserved.

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
// EAWebKitTextWrapper.cpp
// 6/16/09 CSidhall - Wrapper for EAText package for EAWebKit
///////////////////////////////////////////////////////////////////////////////

// Note: Location of this file is currently inside EAWebKit but could be moved to an outside 
// lib if needed as it is dependend on EAText headers.

#include <EAAssert/eaassert.h>
#include <EABase/EABase.h>
#include <EAWebKit/internal/EAWebKitTextWrapper.h>
#include <CoreAllocator/icoreallocator_interface.h>
#include <wtf/FastAllocBase.h>
#include <EAWebKit/internal/EAWebKitAssert.h>
#include <EAWebKit/EAWebKit.h>

// EA Text dependencies
#include <EAText/EATextFontServer.h>   
#include <EAText/EATextFont.h>   
#include <EAText/EATextOutlineFont.h>
#include <EAText/EATextCache.h>
#include <EAText/EATextBreak.h> 
//#include <EAText/D3D/EATextGlyphCache_D3D.h>

//The global variables for local cache and glyph server
static EA::Internal::IGlyphCache*   spGlyphCacheWrapperInterface = NULL; 
static EA::Internal::IFontServer*   spFontServerWrapperInterface = NULL;

namespace EA
{
    namespace TextWrapper
    {

bool gFontSystemHasInit = false;

void InitFontSystem()
{
    if(!gFontSystemHasInit)
    {
        EA::Text::Init();
        gFontSystemHasInit = true;
    }
}

void ShutDownFontSystem()
{
    if(gFontSystemHasInit)
    {
        EA::Text::Shutdown();
        gFontSystemHasInit = false;
    }
}    

//+ Grabbed these following macro/templates EAText (written by Paul Pedriana)
#define EATEXT_WRAPPER_NEW(Class, pAllocator, pName) \
    new ((pAllocator)->Alloc(sizeof(Class), pName, 0, EA_ALIGN_OF(Class), 0)) Class

template <typename T>
inline void delete_object(T* pObject, Allocator::ICoreAllocator* pAllocator)
{
    if(pObject) // As per the C++ standard, deletion of NULL results in a no-op.
    {
        pObject->~T();
        pAllocator->Free(pObject);
    }
}

#define EATEXT_WRAPPER_DELETE(pObject, pAllocator) delete_object(pObject,pAllocator)
//-

// Small helper function to find allocator used
static Allocator::ICoreAllocator* GetAllocator_Helper()
{
    Allocator::ICoreAllocator* pAllocator = EA::Text::GetAllocator();        
    if(!pAllocator)
            pAllocator = Allocator::ICoreAllocator::GetDefaultAllocator();

    return pAllocator;
}

void FontProxy::SetAllocator(Allocator::ICoreAllocator* pCoreAllocator)
{
    EA::Text::Font* pFont = reinterpret_cast<EA::Text::Font*> (mpFont);

    pFont->SetAllocator(pCoreAllocator);         
}

int FontProxy::AddRef()
{
    EA::Text::Font* pFont = reinterpret_cast<EA::Text::Font*> (mpFont);
    mRefCount++;           // Proxy keeps it's own ref count for could be different than font ref

    return pFont->AddRef();
}

int FontProxy::Release()
{
    int returnValue;
    mRefCount--;
    EA::Text::Font* pFont = reinterpret_cast<EA::Text::Font*> (mpFont);
    returnValue =  pFont->Release();
    if((returnValue <= 0) ||(mRefCount <= 0))
    {
        DestroyWrapper();
    }
    return returnValue;
}

bool FontProxy::GetFontMetrics(EA::Internal::FontMetrics& fontMetrics)
{
    EA::Text::Font* pFont = reinterpret_cast<EA::Text::Font*> (mpFont);
    return pFont->GetFontMetrics((EA::Text::FontMetrics&) fontMetrics);
}

bool FontProxy::GetGlyphMetrics(EA::Internal::GlyphId glyphId, EA::Internal::GlyphMetrics& glyphMetrics)
{
    EA::Text::Font* pFont = reinterpret_cast<EA::Text::Font*> (mpFont);
    return pFont->GetGlyphMetrics( (EA::Text::GlyphId) glyphId, (EA::Text::GlyphMetrics&) glyphMetrics);
}

uint32_t FontProxy::GetGlyphIds(const EA::Internal::Char* pCharArray, uint32_t nCharArrayCount, EA::Internal::GlyphId* pGlyphIdArray, 
                             bool bUseReplacementGlyph, const uint32_t nGlyphIdStride, bool bWriteInvalidGlyphs)
{
    EA::Text::Font* pFont = reinterpret_cast<EA::Text::Font*> (mpFont);
    return pFont->GetGlyphIds( (const EA::Text::Char*) pCharArray, nCharArrayCount, (EA::Text::GlyphId*) pGlyphIdArray, 
                                bUseReplacementGlyph, nGlyphIdStride, bWriteInvalidGlyphs);
}

bool FontProxy::IsCharSupported(EA::Internal::Char c, EA::Internal::Script script)
{
    EA::Text::Font* pFont = reinterpret_cast<EA::Text::Font*> (mpFont);
    return pFont->IsCharSupported( (EA::Text::Char) c, (EA::Text::Script) script);
}

float FontProxy::GetSize() const
{
    EA::Text::Font* pFont = reinterpret_cast<EA::Text::Font*> (mpFont);
    return pFont->GetSize();
}

bool FontProxy::RenderGlyphBitmap(const EA::Internal::GlyphBitmap** pGlyphBitmap, EA::Internal::GlyphId g, uint32_t renderFlags, float fXFraction, float fYFraction)
{
    EA::Text::Font* pFont = reinterpret_cast<EA::Text::Font*> (mpFont);
    return pFont->RenderGlyphBitmap( (const EA::Text::Font::GlyphBitmap**) pGlyphBitmap, (EA::Internal::GlyphId) g, renderFlags, fXFraction, fYFraction);
}

void FontProxy::DoneGlyphBitmap(const EA::Internal::GlyphBitmap* pGlyphBitmap)
{
    EA::Text::Font* pFont = reinterpret_cast<EA::Text::Font*> (mpFont);
    pFont->DoneGlyphBitmap((const EA::Text::Font::GlyphBitmap*) pGlyphBitmap);
}

bool FontProxy::GetKerning(EA::Internal::GlyphId g1, EA::Internal::GlyphId g2, EA::Internal::Kerning& kerning, int direction, bool bHorizontalLayout)
{
    EA::Text::Font* pFont = reinterpret_cast<EA::Text::Font*> (mpFont);
    return pFont->GetKerning( (EA::Text::GlyphId) g1, (EA::Text::GlyphId) g2, (EA::Text::Kerning&) kerning, direction, bHorizontalLayout);
}

// This is special for the casted outline font class
bool FontProxy::OpenOutline(const void* pSourceData, uint32_t nSourceSize, int nFaceIndex)
{
    EA::Text::OutlineFont* pOutlineFont = reinterpret_cast<EA::Text::OutlineFont*> (mpFont);                  
    return pOutlineFont->Open(pSourceData, nSourceSize, nFaceIndex);
}

void FontProxy::DestroyWrapper()
{
    if(!mpFont)
         return;

     mpFont = 0;
     Allocator::ICoreAllocator* pAllocator = GetAllocator_Helper();                
     EATEXT_WRAPPER_DELETE(this, pAllocator);     
}

FontProxy::FontProxy()
    :mpFont(0),
    mRefCount(1)
{
} 

FontProxy::FontProxy(void* pFont)
    :mpFont(pFont),
    mRefCount(1)
{
} 

FontProxy::~FontProxy()
{
    if(mpFont)
    {                
        Allocator::ICoreAllocator* pAllocator = GetAllocator_Helper();
        EA::Text::Font* pFont = reinterpret_cast<EA::Text::Font*> (mpFont); 
        EATEXT_WRAPPER_DELETE(pFont, pAllocator);            
        mpFont =0;
    }
}

void FontProxy::SetFontPointer(void* pFont)
{
    mpFont = pFont;
}

void* FontProxy::GetFontPointer() 
{
    return mpFont;
}

EA::Internal::Char* FontStyleProxy::GetFamilyNameArray(int index)
{
    EA::Text::FontStyle* pFontStyle = reinterpret_cast<EA::Text::FontStyle*> (mpFontStyle); 
    return  (EA::Internal::Char*) &pFontStyle->mFamilyNameArray[index];
}    
void FontStyleProxy::SetSize(float value)
{
    EA::Text::FontStyle* pFontStyle = reinterpret_cast<EA::Text::FontStyle*> (mpFontStyle); 
    pFontStyle->mfSize = value;
}

void FontStyleProxy::SetStyle(EA::Internal::Style value)
{
    EA::Text::FontStyle* pFontStyle = reinterpret_cast<EA::Text::FontStyle*> (mpFontStyle); 
    pFontStyle->mStyle = (EA::Text::Style) value;
}

void FontStyleProxy::SetWeight(float value)
{
    EA::Text::FontStyle* pFontStyle = reinterpret_cast<EA::Text::FontStyle*> (mpFontStyle); 
    pFontStyle->mfWeight = value;
}

void FontStyleProxy::SetVariant(EA::Internal::Variant value)
{
    EA::Text::FontStyle* pFontStyle = reinterpret_cast<EA::Text::FontStyle*> (mpFontStyle); 
    pFontStyle->mVariant = (EA::Text::Variant) value;
}

void FontStyleProxy::SetSmooth(EA::Internal::Smooth value)
{
    EA::Text::FontStyle* pFontStyle = reinterpret_cast<EA::Text::FontStyle*> (mpFontStyle); 
    pFontStyle->mSmooth = (EA::Text::Smooth) value;
}

void FontStyleProxy::SetFontStyle(void *pFontStyle)
{
    mpFontStyle = pFontStyle;
}           

void* FontStyleProxy::GetFontStylePointer()
{
    return mpFontStyle;
}

void FontStyleProxy::Destroy()
{
     if(!mpFontStyle)
         return;
     
     Allocator::ICoreAllocator* pAllocator = GetAllocator_Helper();                
     EA::Text::FontStyle* pFontStyle = reinterpret_cast<EA::Text::FontStyle*> (mpFontStyle); 
     EATEXT_WRAPPER_DELETE(pFontStyle, pAllocator);     
     mpFontStyle = 0;
     EATEXT_WRAPPER_DELETE(this, pAllocator);     
}

// Text break iterator statics 
static EA::Text::CharacterBreakIterator gCharacterBreakIterator;
static EA::Text::WordBreakIterator      gWordBreakIterator;
static EA::Text::LineBreakIterator      gLineBreakIterator;
static EA::Text::SentenceBreakIterator  gSentenceBreakIterator;
static EA::Text::TextRun                gTextRun;
static EA::Text::BreakIteratorBase*      gTextBreakIterator;
const int TextBreakDone = -1;
           
EA::Internal::IFont* FontServerProxy::CreateNewFont(int fontType)
{
    EA::Text::FontServer* pServer = reinterpret_cast<EA::Text::FontServer*> (mpServer); 
    
    EA::Text::Font* pFont = pServer->CreateNewFont(fontType);
    return CreateFontProxyWrapper(pFont);
}

EA::Internal::IFont* FontServerProxy::GetFont(EA::Internal::IFontStyle* pTextStyle, EA::Internal::IFont* pFontArray[], uint32_t nFontArrayCapacity, 
                                              EA::Internal::Char c, EA::Internal::Script script, bool bManaged)
{
    EA_ASSERT(!pFontArray); // FontArray is always NULL in EAWebKit
    
    const EA::Text::TextStyle* pFontStyle = (const EA::Text::TextStyle*) pTextStyle->GetFontStylePointer();
    EA::Text::FontServer* pServer = reinterpret_cast<EA::Text::FontServer*> (mpServer); 
    EA::Text::Font* pFont = pServer->GetFont(pFontStyle, NULL, nFontArrayCapacity, (EA::Text::Char) c, (EA::Text::Script) script, bManaged);
    return CreateFontProxyWrapper(pFont);
 }

uint32_t FontServerProxy::EnumerateFonts(EA::Internal::FontDescription* pFontDescriptionArray, uint32_t nCount)
{
    EA::Text::FontServer* pServer = reinterpret_cast<EA::Text::FontServer*> (mpServer); 
    return pServer->EnumerateFonts( (EA::Text::FontDescription*) pFontDescriptionArray, nCount);
}

// Access to some general unicode interface functions
int32_t FontServerProxy::GetCombiningClass(EA::Internal::Char c)
{
    return EA::Text::GetCombiningClass( (EA::Text::Char) c);            
}

EA::Internal::Char FontServerProxy::GetMirrorChar(EA::Internal::Char c)
{
    return (EA::Internal::Char) EA::Text::GetMirrorChar( (EA::Text::Char) c);                 
}

EA::Internal::CharCategory FontServerProxy::GetCharCategory(EA::Internal::Char c)
{
    EA::Text::CharCategory cat = EA::Text::GetCharCategory((EA::Text::Char) c);            
    return (EA::Internal::CharCategory) cat;
}

EA::Internal::BidiClass FontServerProxy::GetBidiClass(EA::Internal::Char c)
{
    EA::Text::BidiClass bidi = EA::Text::GetBidiClass( (EA::Text::Char) c);
        return (EA::Internal::BidiClass) bidi;
}

uint32_t FontServerProxy::GetFamilyNameArrayCapacity()
{
    return EATEXT_FAMILY_NAME_ARRAY_CAPACITY;    
}

uint32_t FontServerProxy::GetFamilyNameCapacity()
{
    return EA::Text::kFamilyNameCapacity;    
}


FontServerProxy::FontServerProxy(void* pServer)
   :mpServer(pServer)
   ,mbOwnFontServer(false)
{
    if(!pServer)
    {
        // Create our own font server inside EAWebKit
    	mbOwnFontServer = true;
		mpServer = WTF::fastNew<EA::Text::FontServer> ();
		EA::Text::FontServer* pFontServer = reinterpret_cast<EA::Text::FontServer*>(mpServer);
		EA::Text::SetFontServer(pFontServer);
       
        //EA::Webkit::SetFontServer(pFontServer);	
        // New to set up the default memory allocator for the font system
        // EA::Text::SetAllocator(pCoreAllocator);
        EA::Text::SetAllocator(Allocator::ICoreAllocator::GetDefaultAllocator());
        


		EAW_ASSERT_MSG(spGlyphCacheWrapperInterface,"Please call CreateGlyphCacheWrapperInterface first and set the default GlyphCache");
		EA::Text::GlyphCache* pGlyphCache = reinterpret_cast<EA::Text::GlyphCache*>(reinterpret_cast<GlyphCacheProxy*>(spGlyphCacheWrapperInterface)->mpGlyphCache);
		EAW_ASSERT_MSG(pGlyphCache,"Please call CreateGlyphCacheWrapperInterface first and set the default GlyphCache");
		pFontServer->SetDefaultGlyphCache(pGlyphCache);
    
    }

    spFontServerWrapperInterface = this;

     EA::WebKit::SetFontServer((EA::Internal::IFontServer*) mpServer);
    
     
     // Since we have some locally exposed const and struct, need to verify they are in sync with EAText
    // If we get an assert here, just need to update the values in EAWebkitTextInterface.h to sync up but that could be a problem
    // for older versions of EAText 
    
    EA_COMPILETIME_ASSERT( sizeof(EA::Text::Char) == sizeof(EA::Internal::Char));
    EA_COMPILETIME_ASSERT( sizeof(EA::Text::GlyphId) == sizeof(EA::Internal::GlyphId));
    EA_COMPILETIME_ASSERT( sizeof(EA::Text::Color) == sizeof(EA::Internal::Color));

    EA_COMPILETIME_ASSERT((int) EA::Text::kCharInvalid == (int) EA::Internal::kCharInvalid);
    EA_COMPILETIME_ASSERT( (int) EA::Text::kGlyphIdInvalid == (int) EA::Internal::kGlyphIdInvalid);
    EA_COMPILETIME_ASSERT( (int) EA::Text::kFamilyNameCapacity == (int) EA::Internal::kFamilyNameCapacity);
    
    EA_COMPILETIME_ASSERT( (int) EA::Text::kPitchDefault == (int) EA::Internal::kPitchDefault);
    EA_COMPILETIME_ASSERT( (int) EA::Text::kPitchVariable == (int) EA::Internal::kPitchVariable);
    EA_COMPILETIME_ASSERT( (int) EA::Text::kPitchFixed == (int) EA::Internal::kPitchFixed);
   
    EA_COMPILETIME_ASSERT( (int) EA::Text::kScriptEnd == (int) EA::Internal::kScriptEnd);
          
    EA_COMPILETIME_ASSERT( (int) EA::Text::kWeightLightest == (int) EA::Internal::kWeightLightest);
    EA_COMPILETIME_ASSERT( (int) EA::Text::kWeightNormal == (int) EA::Internal::kWeightNormal);
    EA_COMPILETIME_ASSERT( (int) EA::Text::kWeightBold == (int) EA::Internal::kWeightBold);
    EA_COMPILETIME_ASSERT( (int) EA::Text::kWeightHeaviest == (int) EA::Internal::kWeightHeaviest);
    
    EA_COMPILETIME_ASSERT( sizeof(EA::Text::GlyphMetrics) == sizeof(EA::Internal::GlyphMetrics));
    EA_COMPILETIME_ASSERT( sizeof(EA::Text::FontMetrics) == sizeof(EA::Internal::FontMetrics));
    
    EA_COMPILETIME_ASSERT( (int) EA::Text::kFontTypeUnknown == (int) EA::Internal::kFontTypeUnknown);
    EA_COMPILETIME_ASSERT( (int) EA::Text::kFontTypeBitmap == (int) EA::Internal::kFontTypeBitmap);
    EA_COMPILETIME_ASSERT( (int) EA::Text::kFontTypeOutline == (int) EA::Internal::kFontTypeOutline);
    EA_COMPILETIME_ASSERT( (int) EA::Text::kFontTypeStroke == (int) EA::Internal::kFontTypeStroke);
    EA_COMPILETIME_ASSERT( (int) EA::Text::kFontTypePolygon == (int) EA::Internal::kFontTypePolygon);

    EA_COMPILETIME_ASSERT( (int) EA::Text::kCCOtherSymbol == (int) EA::Internal::kCCOtherSymbol);
    EA_COMPILETIME_ASSERT( (int) EA::Text::kBidiClassPDF == (int) EA::Internal::kBidiClassPDF);

    EA_COMPILETIME_ASSERT( (int) EA::Text::kStyleNormal == (int) EA::Internal::kStyleNormal);
    EA_COMPILETIME_ASSERT( (int) EA::Text::kStyleOblique == (int) EA::Internal::kStyleOblique);
    EA_COMPILETIME_ASSERT( (int) EA::Text::kStyleItalic == (int) EA::Internal::kStyleItalic);
    EA_COMPILETIME_ASSERT( (int) EA::Text::kStyleDefault == (int) EA::Internal::kStyleDefault);

    EA_COMPILETIME_ASSERT( (int) EA::Text::kVariantNormal == (int) EA::Internal::kVariantNormal);
    EA_COMPILETIME_ASSERT( (int) EA::Text::kVariantSmallCaps == (int) EA::Internal::kVariantSmallCaps);
    EA_COMPILETIME_ASSERT( (int) EA::Text::kVariantDefault == (int) EA::Internal::kVariantDefault);
    
    EA_COMPILETIME_ASSERT( (int) EA::Text::kSmoothNone == (int) EA::Internal::kSmoothNone);
    EA_COMPILETIME_ASSERT( (int) EA::Text::kSmoothEnabled == (int) EA::Internal::kSmoothEnabled);
    EA_COMPILETIME_ASSERT( (int) EA::Text::kSmoothDefault == (int) EA::Internal::kSmoothDefault);

    EA_COMPILETIME_ASSERT( (int) EA::Text::kEffectOutline == (int) EA::Internal::kEffectOutline);
    EA_COMPILETIME_ASSERT( (int) EA::Text::kEffectShadow == (int) EA::Internal::kEffectShadow);
    EA_COMPILETIME_ASSERT( (int) EA::Text::kEffectDefault == (int) EA::Internal::kEffectDefault);
    EA_COMPILETIME_ASSERT( (int) EA::Text::kEffectUser == (int) EA::Internal::kEffectUser);

    EA_COMPILETIME_ASSERT( (int) EA::Text::Font::kRFUnicodeId == (int) EA::Internal::kRFUnicodeId);
    EA_COMPILETIME_ASSERT( (int) EA::Text::Font::kRFDefault == (int) EA::Internal::kRFDefault);

    EA_COMPILETIME_ASSERT( (int) EA::Text::Font::kBFARGB == (int) EA::Internal::kBFARGB);
    EA_COMPILETIME_ASSERT( (int) EA::Text::Font::kBFARGB == (int) EA::Internal::kBFARGB);

    EA_COMPILETIME_ASSERT( sizeof(EA::Text::FontDescription) == sizeof(EA::Internal::FontDescription));
    EA_COMPILETIME_ASSERT( sizeof(EA::Text::Font::GlyphBitmap) == sizeof(EA::Internal::GlyphBitmap));
    EA_COMPILETIME_ASSERT( sizeof(EA::Text::Kerning) == sizeof(EA::Internal::Kerning));
} 


FontServerProxy::~FontServerProxy()
{
	if(mbOwnFontServer)
	{
		// We need to remove the font server since it was created inside EAWebkit
        reinterpret_cast<EA::Text::FontServer*>(mpServer)->Shutdown();
		WTF::fastDelete<EA::Text::FontServer>((reinterpret_cast<EA::Text::FontServer*>(mpServer)));
        EA::WebKit::SetFontServer(0);   
        mbOwnFontServer = false;
    }
    spFontServerWrapperInterface = NULL;
}


EA::Internal::IFontStyle* FontServerProxy::CreateTextStyle()
{
    Allocator::ICoreAllocator* pAllocator = GetAllocator_Helper();                
    EA::Text::FontStyle* pFontStyle=EATEXT_WRAPPER_NEW(EA::Text::TextStyle, pAllocator, "TextStyle");
    if(!pFontStyle)
        return 0;

    FontStyleProxy* pFontStyleProxy = EATEXT_WRAPPER_NEW(FontStyleProxy, pAllocator, "FontStyleProxy");
    if(pFontStyleProxy)
        pFontStyleProxy->SetFontStyle(pFontStyle);
    else
        EATEXT_WRAPPER_DELETE(pFontStyle, pAllocator);    

    return pFontStyleProxy;
}

// FontServer Proxy Class
FontProxy* FontServerProxy::CreateFontProxyWrapper(void* pFont)
{ 
   if(!pFont)
       return 0;     

   Allocator::ICoreAllocator* pAllocator = GetAllocator_Helper();                
   FontProxy* pFontProxy =EATEXT_WRAPPER_NEW(FontProxy, pAllocator, "FontProxy");
   if(pFontProxy)
        pFontProxy->SetFontPointer(pFont);
    else
    {
        EA_ASSERT(0);                
    }
    return pFontProxy;
}


uint32_t FontServerProxy::AddDirectory(const char16_t* pFaceDirectory, const char16_t* pFilter)
{
	EA::Text::FontServer* pServer = reinterpret_cast<EA::Text::FontServer*> (mpServer); 
	return pServer->AddDirectory(pFaceDirectory, pFilter);
}
bool FontServerProxy::AddSubstitution(const char16_t* pFamily, const char16_t* pFamilySubstitution)
{
#if EATEXT_FAMILY_SUBSTITUTION_ENABLED
	EA::Text::FontServer* pServer = reinterpret_cast<EA::Text::FontServer*> (mpServer); 
	return pServer->AddSubstitution(pFamily,pFamilySubstitution);
#endif
	return false;
}


void* FontServerProxy::CharacterBreakIterator(EA::Internal::Char* pText, int length)
{
    // To do: Have EAText iterators keep a member TextRun so you don't need to maintain one externally.
    gTextRun.mpText     = (EA::Text::Char*) pText;
    gTextRun.mnTextSize = (uint32_t)length;
    gCharacterBreakIterator.GetIterator().SetTextRunArray(&gTextRun, 1);
    gTextBreakIterator = &gCharacterBreakIterator;

    return gTextBreakIterator;
}

void* FontServerProxy::WordBreakIterator(EA::Internal::Char* pText, int length)
{
    // To do: Have EAText iterators keep a member TextRun so you don't need to maintain one externally.
    gTextRun.mpText     = (EA::Text::Char*) pText;
    gTextRun.mnTextSize = (uint32_t)length;
    gWordBreakIterator.GetIterator().SetTextRunArray(&gTextRun, 1);
    gTextBreakIterator = &gWordBreakIterator;

    return gTextBreakIterator;
}

void* FontServerProxy::LineBreakIterator(EA::Internal::Char* pText, int length)
{
    // To do: Have EAText iterators keep a member TextRun so you don't need to maintain one externally.
    gTextRun.mpText     = (EA::Text::Char*) pText;
    gTextRun.mnTextSize = (uint32_t)length;
    gLineBreakIterator.GetIterator().SetTextRunArray(&gTextRun, 1);
    gTextBreakIterator = &gLineBreakIterator;

    return gTextBreakIterator;
}

void* FontServerProxy::SentenceBreakIterator(EA::Internal::Char* pText, int length)
{
    // To do: Have EAText iterators keep a member TextRun so you don't need to maintain one externally.
    gTextRun.mpText     = (EA::Text::Char*) pText;
    gTextRun.mnTextSize = (uint32_t)length;
    gSentenceBreakIterator.GetIterator().SetTextRunArray(&gTextRun, 1);
    gTextBreakIterator = &gSentenceBreakIterator;

    return gTextBreakIterator;
}

int FontServerProxy::TextBreakFirst(void* pIter)
{
    EA::Text::BreakIteratorBase* pIterator = reinterpret_cast<EA::Text::BreakIteratorBase*> (pIter);
    // We have a problem: EATextBreak doesn't implement a pure-virtual NextBreak/PrevBreak function.
    // It's easy to do this but it requires us to update EAText. We do something of a hack in the 
    // short run to allow this to work now by checking each of the global variables manually.
    pIterator->SetPosition(0);
    return TextBreakNext(pIter);
}

int FontServerProxy::TextBreakNext(void* pIter)
{
    uint32_t originalPosition;
    uint32_t newPosition;
    EA::Text::BreakIteratorBase* pIterator = reinterpret_cast<EA::Text::BreakIteratorBase*> (pIter);

    if(pIterator == &gCharacterBreakIterator)
    {
        originalPosition = gCharacterBreakIterator.GetPosition();
        newPosition      = gCharacterBreakIterator.GetNextCharBreak();
    }
    else if(pIterator == &gWordBreakIterator)
    {
        originalPosition = gWordBreakIterator.GetPosition();
        newPosition      = gWordBreakIterator.GetNextWordBreak();
    }
    else if(pIterator == &gSentenceBreakIterator)
    {
        originalPosition = gLineBreakIterator.GetPosition();
        newPosition      = gLineBreakIterator.GetNextLineBreak();
    }
    else // if(pIterator == &gSentenceBreakIterator)
    {
        originalPosition = gSentenceBreakIterator.GetPosition();
        newPosition      = gSentenceBreakIterator.GetNextSentenceBreak();
    }

    if(pIterator->AtEnd() && (newPosition == originalPosition))
        return TextBreakDone;

    return (int) newPosition;
}

int FontServerProxy::TextBreakCurrent(void* pIter)
{
    EA::Text::BreakIteratorBase* pIterator = reinterpret_cast<EA::Text::BreakIteratorBase*> (pIter);

    return (int)pIterator->GetPosition();
}

int FontServerProxy::TextBreakPreceding(void* pIter, int position)
{
    EA::Text::BreakIteratorBase* pIterator = reinterpret_cast<EA::Text::BreakIteratorBase*> (pIter);

    uint32_t originalPosition;
    uint32_t newPosition;

    pIterator->SetPosition((uint32_t)position);

    if(pIterator == &gCharacterBreakIterator)
    {
        originalPosition = gCharacterBreakIterator.GetPosition();
        newPosition      = gCharacterBreakIterator.GetPrevCharBreak();
    }
    else if(pIterator == &gWordBreakIterator)
    {
        originalPosition = gWordBreakIterator.GetPosition();
        newPosition      = gWordBreakIterator.GetPrevWordBreak();
    }
    else if(pIterator == &gSentenceBreakIterator)
    {
        originalPosition = gLineBreakIterator.GetPosition();
        newPosition      = gLineBreakIterator.GetPrevLineBreak();
    }
    else // if(pIterator == &gSentenceBreakIterator)
    {
        originalPosition = gSentenceBreakIterator.GetPosition();
        newPosition      = gSentenceBreakIterator.GetPrevSentenceBreak();
    }

    if(pIterator->AtBegin() && (newPosition == originalPosition))
        return TextBreakDone;

return (int) newPosition;
}

int FontServerProxy::TextBreakFollowing(void* pIter, int position)
{
    EA::Text::BreakIteratorBase* pIterator = reinterpret_cast<EA::Text::BreakIteratorBase*> (pIter);
    pIterator->SetPosition((uint32_t)position);
    return TextBreakNext(pIter);
}

bool FontServerProxy::IsTextBreak(void* pIter, int position)
{
     EA::Text::BreakIteratorBase* pIterator = reinterpret_cast<EA::Text::BreakIteratorBase*> (pIter);

    // If we are being asked about the most recent break position, then we know it is a break position.
    // Otherwise the only means we have to implementing this is to walk through the entire string
    // and check each break against position.
    if(position == (int)pIterator->GetPosition())
        return true;

    pIterator->SetPosition(0);

    while(((int)pIterator->GetPosition() < position) && !pIterator->AtEnd())
        TextBreakNext(pIterator);

    return (position == (int)pIterator->GetPosition());
}

int FontServerProxy::FindNextWordFromIndex(EA::Internal::Char* chars, int len, int position, bool forward)
{
    /// The arrows below show the positions that would be returned by 
    /// successive calls to GetNextWordBreak with the default wordBreakType.
    ///    "Hello   world   hello   world"
    ///     ^    ^  ^    ^  ^    ^  ^    ^

    EA::Text::TextRun           tr((EA::Text::Char*)chars, (uint32_t)len);
    EA::Text::WordBreakIterator wbi;

    wbi.SetIterator(EA::Text::TextRunIterator(&tr));
    wbi.SetPosition((uint32_t)position);

    if(forward)
        return (int)wbi.GetNextWordBreak(EA::Text::kWordBreakTypeBegin);    // It turns out that as of this writing EAText doesn't implement kWordBreakTypeBegin/End. We will need to make sure that's working so this code can work as expected.
    else
        return (int)wbi.GetPrevWordBreak(EA::Text::kWordBreakTypeBegin);
}

void FontServerProxy::FindWordBoundary(EA::Internal::Char* chars, int len, int position, int* start, int* end)
{
    /// The arrows below show the positions that would be returned by 
    /// successive calls to GetNextWordBreak with the default wordBreakType.
    ///    "Hello   world   hello   world"
    ///     ^    ^  ^    ^  ^    ^  ^    ^

    EA::Text::TextRun           tr((EA::Text::Char*) chars, (uint32_t)len);
    EA::Text::WordBreakIterator wbi;

    wbi.SetIterator(EA::Text::TextRunIterator(&tr));
    wbi.SetPosition((uint32_t)position);

    *start = (int)wbi.GetPrevWordBreak(EA::Text::kWordBreakTypeBegin);

    wbi.SetPosition((uint32_t)*start);

    // WebKit may expect that 'end' refer to the last char and not one-past it.
    // Also, it may expect that end not refer to the end of the space after word and instead mean the very end of the word chars.
    *end = (int)wbi.GetNextWordBreak(EA::Text::kWordBreakTypeEnd) - 1;
}

int TextureInfoProxy::AddRef()
{
    EA::Text::TextureInfo* pInfo = reinterpret_cast<EA::Text::TextureInfo*> (mpInfo);
    return pInfo->AddRef();
}

int TextureInfoProxy::Release()
{
    EA::Text::TextureInfo* pInfo = reinterpret_cast<EA::Text::TextureInfo*> (mpInfo);
    return pInfo->Release();
}

uint32_t TextureInfoProxy::GetSize()
{
    EA::Text::TextureInfo* pInfo = reinterpret_cast<EA::Text::TextureInfo*> (mpInfo);
    return pInfo->mnSize;
}

intptr_t TextureInfoProxy::GetStride()
{
    EA::Text::TextureInfo* pInfo = reinterpret_cast<EA::Text::TextureInfo*> (mpInfo);
    return pInfo->mnStride;
}

uint8_t* TextureInfoProxy::GetData()
{
    EA::Text::TextureInfo* pInfo = reinterpret_cast<EA::Text::TextureInfo*> (mpInfo);
    return pInfo->mpData;
}

void TextureInfoProxy::DestroyWrapper()
{
    if(!mpInfo)
     return;

    mpInfo = NULL;
    Allocator::ICoreAllocator* pAllocator = GetAllocator_Helper();                
    EATEXT_WRAPPER_DELETE(this, pAllocator);     
}

void TextureInfoProxy::SetTextureInfo(void *pInfo)
{
    mpInfo = pInfo;
}

void* TextureInfoProxy::GetTextureInfoPointer() 
{
    return mpInfo;
}

EA::Internal::ITextureInfo* GlyphCacheProxy::GetTextureInfo(uint32_t nTextureIndex)
{
    EA::Text::GlyphCache* pGlyphCache = reinterpret_cast<EA::Text::GlyphCache*> (mpGlyphCache);
    EA::Text::TextureInfo* pInfo = pGlyphCache->GetTextureInfo(nTextureIndex);
    EA_ASSERT_MESSAGE(pInfo, "Please make sure that you have initialized GlyphCache so that nTextureIndex falls inside the number of textures that GlyphCache has.");
	if(!pInfo) 
        return NULL;

    // Allocate proxy
    return CreateTextureInfoProxy(pInfo);
}

bool GlyphCacheProxy::GetGlyphTextureInfo(EA::Internal::IFont* pFont, EA::Internal::GlyphId glyphId,EA::Internal::IGlyphTextureInfo& glyphTextureInfo) 
{
    
    EA::Text::Font* pFontEA =  (EA::Text::Font *) pFont->GetFontPointer();
    EA::Text::GlyphTextureInfo  glyphTextureInfoEA;
    EA::Text::GlyphCache* pGlyphCache = reinterpret_cast<EA::Text::GlyphCache*> (mpGlyphCache);

    bool returnFlag = pGlyphCache->GetGlyphTextureInfo(pFontEA, (EA::Text::GlyphId) glyphId, glyphTextureInfoEA);
    if(returnFlag)
    {
        // Create a wrapper for the texture info                            
        glyphTextureInfo.mpTextureInfo = CreateTextureInfoProxy(glyphTextureInfoEA.mpTextureInfo);
        glyphTextureInfo.mX1 = glyphTextureInfoEA.mX1; 
        glyphTextureInfo.mY1 = glyphTextureInfoEA.mY1; 
        glyphTextureInfo.mX2 = glyphTextureInfoEA.mX2; 
        glyphTextureInfo.mY2 = glyphTextureInfoEA.mY2; 
    }
    
    return returnFlag;
}

bool GlyphCacheProxy::AddGlyphTexture(EA::Internal::IFont* pFont, EA::Internal::GlyphId glyphId, const void* pSourceData, uint32_t nSourceSizeX, 
                             uint32_t nSourceSizeY, uint32_t nSourceStride, uint32_t nSourceFormat,
                             EA::Internal::IGlyphTextureInfo& glyphTextureInfo)
{
   
    EA::Text::Font* pFontEA =  (EA::Text::Font *) pFont->GetFontPointer();
    EA::Text::GlyphTextureInfo  glyphTextureInfoEA;
    EA::Text::GlyphCache* pGlyphCache = reinterpret_cast<EA::Text::GlyphCache*> (mpGlyphCache);
    bool returnFlag = pGlyphCache->AddGlyphTexture(pFontEA, (EA::Text::GlyphId) glyphId, pSourceData, nSourceSizeX, 
                             nSourceSizeY, nSourceStride, nSourceFormat, glyphTextureInfoEA);
    if(returnFlag)
    {
        // Create a wrapper for the texture info                            
        glyphTextureInfo.mpTextureInfo = CreateTextureInfoProxy(glyphTextureInfoEA.mpTextureInfo);
        glyphTextureInfo.mX1 = glyphTextureInfoEA.mX1; 
        glyphTextureInfo.mY1 = glyphTextureInfoEA.mY1; 
        glyphTextureInfo.mX2 = glyphTextureInfoEA.mX2; 
        glyphTextureInfo.mY2 = glyphTextureInfoEA.mY2; 
    }
    return returnFlag;            
}

bool GlyphCacheProxy::EndUpdate(EA::Internal::ITextureInfo* pTextureInfo)
{

    EA::Text::TextureInfo* pTextureInfoEA = (EA::Text::TextureInfo*) pTextureInfo->GetTextureInfoPointer();
    EA::Text::GlyphCache* pGlyphCache = reinterpret_cast<EA::Text::GlyphCache*> (mpGlyphCache);
    pGlyphCache->EndUpdate(pTextureInfoEA);
    return true;

}

TextureInfoProxy* GlyphCacheProxy::CreateTextureInfoProxy(void* pInfo)
{
    Allocator::ICoreAllocator* pAllocator = GetAllocator_Helper();                
    TextureInfoProxy* pTextureInfoProxy =EATEXT_WRAPPER_NEW(TextureInfoProxy, pAllocator, "TextureInfoProxy");
    if(pTextureInfoProxy)
        pTextureInfoProxy->SetTextureInfo(pInfo);
    return pTextureInfoProxy;
}

GlyphCacheProxy::GlyphCacheProxy(void* pGlypheCache)
   : mpGlyphCache(pGlypheCache)
   , mbOwnGlyphCache(false)
{
	if(!mpGlyphCache) //If Glyph Cache was not passed to us, we will create ours.
	{
	    EAW_ASSERT_MSG(!spGlyphCacheWrapperInterface, "Glyph Cache wrapper interface is alreday created!");
	          
        InitFontSystem();
		
        mbOwnGlyphCache = true;
		mpGlyphCache = WTF::fastNew<EA::Text::GlyphCache_Memory> (EA::Text::kTextureFormat8Bpp);

        EA::Text::GlyphCache* pGlyphCache = reinterpret_cast<EA::Text::GlyphCache*>(mpGlyphCache);
		pGlyphCache->SetAllocator(EA::Text::GetAllocator());
		pGlyphCache->SetOption(EA::Text::GlyphCache::kOptionDefaultSize, 1024);
		pGlyphCache->SetOption(EA::Text::GlyphCache::kOptionDefaultFormat, EA::Text::kTextureFormat8Bpp);
		const int result = pGlyphCache->Init(2, 1); // (nMaxTextureCount = 1, nInitialTextureCount = 1)
		EAW_ASSERT(result == 1); 
		(void)result;

     }
    spGlyphCacheWrapperInterface = this;
} 

GlyphCacheProxy::~GlyphCacheProxy()
{
	if(mbOwnGlyphCache)
	{
		// We need to remove the GlypyCache since it was created inside EAWebKit.
        reinterpret_cast<EA::Text::GlyphCache*>(mpGlyphCache)->Shutdown();
		WTF::fastDelete<EA::Text::GlyphCache>((reinterpret_cast<EA::Text::GlyphCache*>(mpGlyphCache)));
        mbOwnGlyphCache = false;
    }
  	spGlyphCacheWrapperInterface = NULL;

}

    } // Namespace TextWrapper
} // Namespace EA

