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
// EAWebKitTextWrapper.h
// 6/16/09 CSidhall - Wrapper for EAText package 
///////////////////////////////////////////////////////////////////////////////

// Note: Location of this file is currently inside EAWebKit but could be moved to an outside lib or 
// the outside app if needed

#ifndef EAWEBKIT_TEXT_WRAPPER_H
#define EAWEBKIT_TEXT_WRAPPER_H

#include <EABase/EABase.h>
#include <EAWebKit/EAWebKitTextInterface.h>
#include <EAWebKit/EAWebKitConfig.h>
#if EAWEBKIT_THROW_BUILD_ERROR
#error This file should be included only in a dll build
#endif


// Include Note: We don't want to include EAText headers here but use them instead in the cpp file as this isolates EAText from EAWebkit.

namespace EA
{
    namespace TextWrapper
    {
        void InitFontSystem();
        void ShutDownFontSystem();

        // Wrapper class for the IFont base class. 
        class FontProxy : public EA::Internal::IFont
        {
        public:
        
           FontProxy();
           FontProxy(void* pFont);
           ~FontProxy();

            void SetAllocator(Allocator::ICoreAllocator* pCoreAllocator);
            int AddRef();
            int Release();
            bool GetFontMetrics(EA::Internal::FontMetrics& fontMetrics);
            bool GetGlyphMetrics(EA::Internal::GlyphId glyphId, EA::Internal::GlyphMetrics& glyphMetrics);
            uint32_t GetGlyphIds(const EA::Internal::Char* pCharArray, uint32_t nCharArrayCount, EA::Internal::GlyphId* pGlyphIdArray = NULL, 
                bool bUseReplacementGlyph = true, const uint32_t nGlyphIdStride = sizeof(EA::Internal::GlyphId), bool bWriteInvalidGlyphs = false);

            bool IsCharSupported(EA::Internal::Char c, EA::Internal::Script script = EA::Internal::kScriptUnknown);
            float GetSize() const;
            bool RenderGlyphBitmap(const EA::Internal::GlyphBitmap** pGlyphBitmap, EA::Internal::GlyphId g, uint32_t renderFlags = EA::Internal::kRFDefault, float fXFraction = 0, float fYFraction = 0);
            void DoneGlyphBitmap(const EA::Internal::GlyphBitmap* pGlyphBitmap);
            bool GetKerning(EA::Internal::GlyphId g1, EA::Internal::GlyphId g2, EA::Internal::Kerning& kerning, int direction, bool bHorizontalLayout = true);

            // This is for the casted outline font class
            bool OpenOutline(const void* pSourceData, uint32_t nSourceSize, int nFaceIndex = 0);

            // Wrapper utilities
            void DestroyWrapper();
            void SetFontPointer(void* pFont);  // Basically the EA::Text::Font when using EAText
            void* GetFontPointer(); 

        private:
            void* mpFont;   // Basically the EA::Text::Font proxy
            int mRefCount;  // Needs its own refcount for can be different than the Font ref count because set up outside of EAWebKit
        };

        // Wrapper structure for IFontStyle
        struct FontStyleProxy : EA::Internal::IFontStyle
        {
            EA::Internal::Char* GetFamilyNameArray(int index);
            void SetSize(float value);
            void SetStyle(EA::Internal::Style value);
            void SetWeight(float value);
            void SetVariant(EA::Internal::Variant value);
            void SetSmooth(EA::Internal::Smooth value);
            void SetFontStyle(void* pFontStyle);
            void* GetFontStylePointer();
            void Destroy();

            void* mpFontStyle;   // = EA::Text::FontStyle proxy when using EAText
        };

        // Wrapper for the IFontServer
        class FontServerProxy : public EA::Internal::IFontServer
        {
        public:
            FontServerProxy(void* pServer = NULL); //If a NULL is passes, we'll use EAWebkit's copy of EAText to create a FontServer
            ~FontServerProxy();
			FontProxy* CreateFontProxyWrapper(void* pFont);

            EA::Internal::IFont* CreateNewFont(int fontType);
            EA::Internal::IFont* GetFont(EA::Internal::IFontStyle* pTextStyle, EA::Internal::IFont* pFontArray[] = NULL, uint32_t nFontArrayCapacity = 0, 
                    EA::Internal::Char c = EA::Internal::kCharInvalid, EA::Internal::Script script = EA::Internal::kScriptUnknown, bool bManaged = true);

            uint32_t EnumerateFonts(EA::Internal::FontDescription* pFontDescriptionArray, uint32_t nCount);
			uint32_t AddDirectory(const char16_t* pFaceDirectory, const char16_t* pFilter = NULL);
			virtual bool AddSubstitution(const char16_t* pFamily, const char16_t* pFamilySubstitution);

            // Font Style interface wrapper
            EA::Internal::IFontStyle* CreateTextStyle();

            // Access to some general unicode interface functions
            int32_t GetCombiningClass(EA::Internal::Char c);
            EA::Internal::Char GetMirrorChar(EA::Internal::Char c);
            EA::Internal::CharCategory GetCharCategory(EA::Internal::Char c);
            EA::Internal::BidiClass GetBidiClass(EA::Internal::Char c);
            uint32_t GetFamilyNameArrayCapacity();
            uint32_t GetFamilyNameCapacity();
          
            // Text break iterator access functions
            void* CharacterBreakIterator(EA::Internal::Char* pText, int length);
            void* WordBreakIterator(EA::Internal::Char* pText, int length);
            void* LineBreakIterator(EA::Internal::Char* pText, int length);
            void* SentenceBreakIterator(EA::Internal::Char* pText, int length);
            int TextBreakFirst(void* pIter);
            int TextBreakNext(void* pIter);
            int TextBreakCurrent(void* pIter);
            int TextBreakPreceding(void* pIter, int position);
            int TextBreakFollowing(void* pIter, int position);
            bool IsTextBreak(void* pIter, int position);
            int FindNextWordFromIndex(EA::Internal::Char* chars, int len, int position, bool forward);
            void FindWordBoundary(EA::Internal::Char* chars, int len, int position, int* start, int* end);

        private:
            void* mpServer; // = EA::Text::FontServer proxy when using EAText
			bool mbOwnFontServer;//true if we create ours

        };       

        // Wrapper for the ITextureInfo
        struct TextureInfoProxy : EA::Internal::ITextureInfo  
	    {
            void SetTextureInfo(void *pInfo);

            int AddRef();
            int Release();
            uint32_t GetSize();
            intptr_t GetStride();
            uint8_t* GetData();
             void DestroyWrapper();
            void* GetTextureInfoPointer();

            void* mpInfo;   // = EA::Text::TextureInfo proxy when using EAText
        };

        // Wrapper for the IGlyphCache base class
        class GlyphCacheProxy : public EA::Internal::IGlyphCache
        {
			friend class FontServerProxy;
        public:
			GlyphCacheProxy(void* pGlypheCache = NULL);//If a NULL is passes, we'll use EAWebkit's copy of EAText to create GlyphCache
            ~GlyphCacheProxy();
            EA::Internal::ITextureInfo* GetTextureInfo(uint32_t nTextureIndex);
            bool GetGlyphTextureInfo(EA::Internal::IFont* pFont, EA::Internal::GlyphId glyphId,EA::Internal::IGlyphTextureInfo& glyphTextureInfo); 
            bool AddGlyphTexture(EA::Internal::IFont* pFont, EA::Internal::GlyphId glyphId, const void* pSourceData, uint32_t nSourceSizeX, 
                                         uint32_t nSourceSizeY, uint32_t nSourceStride, uint32_t nSourceFormat,
                                         EA::Internal::IGlyphTextureInfo& glyphTextureInfo);

            bool EndUpdate(EA::Internal::ITextureInfo* pTextureInfo);
            TextureInfoProxy* CreateTextureInfoProxy(void* pInfo);

        private:
            void* mpGlyphCache; // = EA::Text::GlyphCache proxy when using EAText
			bool mbOwnGlyphCache;//true if we create ours

       };

    } // Namespace TextWrapper

} // Namespace EA
#endif // Header include guard

