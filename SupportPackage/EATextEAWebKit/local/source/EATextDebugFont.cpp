/*
Copyright (C) 2007,2009 Electronic Arts, Inc.  All rights reserved.

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
// EATextDebugFont.cpp
// By Paul Pedriana - 2007
//
///////////////////////////////////////////////////////////////////////////////


#include <EAText/EATextDebugFont.h>
#include EA_ASSERT_HEADER


namespace EA
{

namespace Text
{




///////////////////////////////////////////////////////////////////////////////
// DebugFont
///////////////////////////////////////////////////////////////////////////////

DebugFont::DebugFont(Allocator::ICoreAllocator* pCoreAllocator)
    : OutlineFont(pCoreAllocator)
{
    memset(mGlyphMetricsArray, 0, sizeof(mGlyphMetricsArray));
}


bool DebugFont::SetTransform(float fSize, float fAngle, float fSkew)
{
    // We intercept SetTransform here and call SetupGlyphMetrics. 
    // We do this here because it is after SetTransform that the 
    // font is first usable.

    if(OutlineFont::SetTransform(fSize, fAngle, fSkew))
    {
        SetupGlyphMetrics();
        return true;
    }

    return false;
}

void DebugFont::SetupGlyphMetrics()
{
    for(Char c = kCharMin; c < kCharEnd; ++c)
    {
        GlyphId g;
        OutlineFont::GetGlyphIds(&c, 1, &g, true);
        OutlineFont::GetGlyphMetrics(g, mGlyphMetricsArray[c - kCharMin]);
    }
}

} // namespace Text

} // namespace EA











