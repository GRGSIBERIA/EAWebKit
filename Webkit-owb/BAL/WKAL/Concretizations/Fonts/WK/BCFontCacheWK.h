/*
 * Copyright (C) 2006, 2008 Apple Computer, Inc.  All rights reserved.
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

#ifndef FontCache_h
#define FontCache_h

#include <wtf/FastAllocBase.h>
#include <limits.h>
#include <wtf/Vector.h>
#include <wtf/unicode/Unicode.h>

#if PLATFORM(WIN)
#include <objidl.h>
#include <mlang.h>
#endif

namespace WKAL{

class AtomicString;
class Font;
class FontPlatformData;
class FontData;
class FontDescription;
class FontSelector;
class SimpleFontData;

class FontCache: public WTF::FastAllocBase {
public:
//+daw ca 24/07 static and glbal management
	void static staticFinalize();
private:
	const static AtomicString* alternateFamilyName(const AtomicString& familyName);
	static AtomicString* m_st_pCourier;
	static AtomicString* m_st_pCourierNew;
	static AtomicString* m_st_pTimes;
	static AtomicString* m_st_pTimesNewRoman;
	static AtomicString* m_st_pArial;
	static AtomicString* m_st_pHelvetica;
public:
//-daw ca

    static const FontData* getFontData(const Font&, int& familyIndex, FontSelector*);
    static void releaseFontData(const SimpleFontData*);
    
    // This method is implemented by the platform.
    // FIXME: Font data returned by this method never go inactive because callers don't track and release them.
    static const SimpleFontData* getFontDataForCharacters(const Font&, const UChar* characters, int length);
    
    // Also implemented by the platform.
    static void platformInit();

#if PLATFORM(WIN)
    static IMLangFontLink2* getFontLinkInterface();
#endif

    static void getTraitsInFamily(const AtomicString&, Vector<unsigned>&);

    static FontPlatformData* getCachedFontPlatformData(const FontDescription&, const AtomicString& family, bool checkingAlternateName = false);
    static SimpleFontData* getCachedFontData(const FontPlatformData*);
    static FontPlatformData* getLastResortFallbackFont(const FontDescription&);

    static size_t fontDataCount();
    static size_t inactiveFontDataCount();
    static void purgeInactiveFontData(int count = INT_MAX);

private:
    // These methods are implemented by each platform.
    static FontPlatformData* getSimilarFontPlatformData(const Font&);
    static FontPlatformData* createFontPlatformData(const FontDescription&, const AtomicString& family);

    friend class SimpleFontData;
    friend class FontFallbackList;
};

}

#endif
