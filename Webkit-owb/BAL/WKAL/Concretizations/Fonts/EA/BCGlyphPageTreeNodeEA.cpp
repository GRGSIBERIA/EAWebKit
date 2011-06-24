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
// BCGlyphPageTreeNodeEA.cpp
// By Paul Pedriana
///////////////////////////////////////////////////////////////////////////////


#include "config.h"
#include "GlyphPageTreeNode.h"
#include "BALBase.h"
#include "SimpleFontData.h"
#include <EAWebKit/EAWebKitTextInterface.h>  


namespace WKAL {

// We have:
//
// struct GlyphData {
//     Glyph glyph;
//     const SimpleFontData* pSimpleFontData;
// };
// 
// struct GlyphPage : public RefCounted<GlyphPage> {
//     static const size_t size = 256;
//     GlyphData m_glyphs[size];
//     . . .
// }


// This function returns true if the SimpleFontData has any of the glyphs
// associated with buffer/bufferLength.
bool GlyphPage::fill(unsigned offset, unsigned length, UChar* buffer, unsigned bufferLength, const SimpleFontData* pSimpleFontData)
{
    bool haveGlyphs = false;

   // Is this always true?
    if (bufferLength <= WKAL::GlyphPage::size)
    {
        EA::Internal::IFont* const pFont = pSimpleFontData->m_font.mpFont;

        if (pFont)
        {
            for (unsigned i = 0; i < bufferLength; i++)
            {
                EA::Internal::GlyphId glyphId = EA::Internal::kGlyphIdInvalid;

                pFont->GetGlyphIds(&buffer[i], 1, &glyphId, false);

                if (glyphId != EA::Internal::kGlyphIdInvalid)
                {
                    setGlyphDataForIndex(i, glyphId, pSimpleFontData);
                    haveGlyphs = true;
                }
                else
                    setGlyphDataForIndex(i, 0, 0);
            }
        }
    }
    return haveGlyphs;
}


} // namespace WKAL
