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
// BCFontEA.cpp
// By Paul Pedriana
///////////////////////////////////////////////////////////////////////////////


#include "config.h"
#include "Font.h"
#include "GraphicsContext.h"
#include "SimpleFontData.h"
#include "IntSize.h"
#include <EASTL/fixed_string.h>
#include "EARaster.h"
#include <EAWebKit/EAWebKit.h>
#include <EAWebKit/EAWebKitTextInterface.h>  
#include <EAWebKit/internal/EAWebKitAssert.h>

namespace WKAL {


// This is somewhat like EAText GlyphLayoutInfo, but since WebKit does the layout on
// its own, it's more efficient if we use a struct tailored to how WebKit works.
// The situation here is a little simpler than with EAText because WebKit always 
// calls the drawGlyphs function with a span that uses a single font. We also make
// the assumption below that a single glyph cache texture handles all glyphs at 
#include <wtf/FastAllocBase.h>
// at time.
struct GlyphDrawInfo: public WTF::FastAllocBase
{
    int x1;     // Destination glyph box x left
    int x2;     // Destination glyph box x right
    int y1;     // Destination glyph box y top
    int y2;     // Destination glyph box y bottom
    int tx;     // Texture glyph box x left
    int ty;     // Texture glyph box y top
};

typedef Vector<GlyphDrawInfo, 128> GlyphDrawInfoArray;


#if defined(EA_DEBUG) && defined(AUTHOR_PPEDRIANA_DISABLED)
    // This is slow, but it's debug-only.
    char16_t ConvertGlyphToChar(EA::Text::Font* pFont, EA::Text::GlyphId glyphId)
    {
        EA::Text::GlyphId glyphIdResult;

        for(char16_t c = 0x20; c < 0x7f; ++c)
        {
            pFont->GetGlyphIds(&c, 1, &glyphIdResult, false);

            if(glyphIdResult == glyphId)
                return c;
        } 

        return '?';
    }

    void TraceGlyphBufferToString(const SimpleFontData* pSimpleFontData, const GlyphBuffer& glyphBuffer, int glyphIndexBegin, int glyphCount, const FloatPoint& point)
    {
        eastl::fixed_string<char8_t, 128> sDebug;
        sDebug.sprintf("[%d,%d] ", (int)point.x(), (int)point.y());

        GlyphBufferGlyph* pGlyphArray = const_cast<GlyphBufferGlyph*>(glyphBuffer.glyphs(glyphIndexBegin));

        for(int i = glyphIndexBegin; i < glyphIndexBegin + glyphCount; ++i, ++pGlyphArray)
            sDebug += (char8_t)ConvertGlyphToChar(pSimpleFontData->m_font.mpFont, *pGlyphArray);

        sDebug += "\n";

       OWB_PRINTF("%s", sDebug.c_str());
    }
#endif

// We need to draw the glyphBuffer glyphs/advances with the pSimpleFontData font onto the 
// pGraphicsContext pSurface. We draw glyphCount glyphs starting at the glyphIndexBegin.
void Font::drawGlyphs(GraphicsContext* pGraphicsContext, const SimpleFontData* pSimpleFontData, const GlyphBuffer& glyphBuffer,
                        int glyphIndexBegin, int glyphCount, const FloatPoint& point) const
{
    // 11/09/09 CSidhall Added notify start of process to user
	NOTIFY_PROCESS_STATUS(EA::WebKit::kVProcessTypeDrawGlyph, EA::WebKit::kVProcessStatusStarted);
	
    #if defined(EA_DEBUG) && defined(AUTHOR_PPEDRIANA_DISABLED) && defined(EA_PLATFORM_WINDOWS)
        TraceGlyphBufferToString(pSimpleFontData, glyphBuffer, glyphIndexBegin, glyphCount, point);
    #endif

    GlyphBufferGlyph*               glyphs      = const_cast<GlyphBufferGlyph*>(glyphBuffer.glyphs(glyphIndexBegin));
    float                           offset      = 0;   
    int                             x_offset    = 0;
    EA::Internal::IFont*            pFont       = pSimpleFontData->m_font.mpFont;
    EA::Internal::IGlyphCache*      pGlyphCache = EA::WebKit::GetGlyphCache(); 
    EAW_ASSERT_MSG(pGlyphCache, "GlyphCache is not set");
	if(!pGlyphCache)
	{
		NOTIFY_PROCESS_STATUS(EA::WebKit::kVProcessTypeDrawGlyph, EA::WebKit::kVProcessStatusEnded);
		return;
	}
    
    EA::Internal::ITextureInfo*     pTI         = pGlyphCache->GetTextureInfo(0); // Right now we hard-code usage of what is the only texture.
	EAW_ASSERT_MSG(pTI, "GlyphCache is not initialized with enough number of textures");
	if(!pTI)
	{
		NOTIFY_PROCESS_STATUS(EA::WebKit::kVProcessTypeDrawGlyph, EA::WebKit::kVProcessStatusEnded);
		return;
	}

	const int32_t                   textureSize = (int32_t)pTI->GetSize();
    EA::Internal::IGlyphTextureInfo gti;
    EA::Internal::GlyphMetrics      glyphMetrics;
    GlyphDrawInfoArray              gdiArray((size_t)(unsigned)glyphCount);

    // Walk through the list of glyphs and build up render info for each one.
    for (int i = 0; i < glyphCount; i++)
    {
        EA::Internal::GlyphId g = glyphs[i];

        if(!pFont->GetGlyphMetrics(g, glyphMetrics))
        {
            EAW_ASSERT_MSG(false, "Font::drawGlyphs: invalid glyph/Font combo.");
            pFont->GetGlyphIds(L"?", 1, &g, true);
            pFont->GetGlyphMetrics(g, glyphMetrics);
        }

        if(!pGlyphCache->GetGlyphTextureInfo(pFont, g, gti))
        {
            const EA::Internal::GlyphBitmap* pGlyphBitmap;

            if(pFont->RenderGlyphBitmap(&pGlyphBitmap, g))
            {
                if(pGlyphCache->AddGlyphTexture(pFont, g, pGlyphBitmap->mpData, pGlyphBitmap->mnWidth, pGlyphBitmap->mnHeight, 
                                                              pGlyphBitmap->mnStride, (uint32_t)pGlyphBitmap->mBitmapFormat, gti))
                {
                    pGlyphCache->EndUpdate(gti.mpTextureInfo);
                }
                else
                {
                    EAW_ASSERT_MSG(false, "Font::drawGlyphs: AddGlyphTexture failed.");
                    gti.mX1 = gti.mX2 = 0; // Make this an empty glyph. Normally this should never execute.
                }

                pFont->DoneGlyphBitmap(pGlyphBitmap);
            } 
            else
            {
                EAW_ASSERT_MSG(false, "Font::drawGlyphs: invalid glyph/Font combo.");
                gti.mX1 = gti.mX2 = 0; // Make this an empty glyph. Normally this should never execute.
            }
        }

        // Apply kerning.
        // Note by Paul Pedriana: Can we really apply kerning here at the render stage without it looking 
        // wrong? It seems to me this cannot work unless kerning is also taken into account at the 
        // layout stage. To do: See if in fact kerning is taken into account at the layout stage.
        EA::Internal::Kerning kerning;

        if((i > 0) && pFont->GetKerning(glyphs[i - 1], g, kerning, 0))
            offset += kerning.mfKernX;

        // The values we calculate here are relative to the current pen position at the 
        // baseline position of [xoffset, 0], where +X is rightward and +Y is upward.
        gdiArray[i].x1 = (int)(offset + glyphMetrics.mfHBearingX);
        gdiArray[i].x2 = (int)(offset + glyphMetrics.mfHBearingX + glyphMetrics.mfSizeX);
        gdiArray[i].y1 = (int)(glyphMetrics.mfHBearingY);
        gdiArray[i].y2 = (int)(glyphMetrics.mfHBearingY - glyphMetrics.mfSizeY);
        gdiArray[i].tx = (int)(gti.mX1 * textureSize); // Convert [0..1] to [0..textureSize]
        gdiArray[i].ty = (int)(gti.mY1 * textureSize);

        // advanceAt should return a value that is usually equivalent to glyphMetrics.mfHAdvanceX, at least 
        // for most simple Western text. A case where it would be different would be Arabic combining glyphs,
        // and custom kerning, though kerning adjustments are handled above.
        offset += glyphBuffer.advanceAt(glyphIndexBegin + i);
    
        // Free wrapper
        if(gti.mpTextureInfo)
            gti.mpTextureInfo->DestroyWrapper();
    
    }

	// Find the X and Y minima and maxima of the glyph boxes.
    // To do: We can improve the efficiency of this code in a couple of ways, including moving the 
    // min/max tests up into the glyphMetrics code above.
    int xMin = gdiArray[0].x1;
    int xMax = gdiArray[0].x2;
    int yMin = gdiArray[0].y2;    // y2 is min because y goes upward.
    int yMax = gdiArray[0].y1;

    for (GlyphDrawInfoArray::const_iterator it = gdiArray.begin(); it != gdiArray.end(); ++it)
    {
        const GlyphDrawInfo& gdi = *it;

        // We assume tha x1 is always <= x2.
        if (gdi.x1 < xMin)
            xMin = gdi.x1;

        if (gdi.x2 > xMax)
            xMax = gdi.x2;

        // if (gdi.y1 < yMin)  // We normally don't need this check, because y2 will usually (or always?) be less than y1.
        //     yMin = gdi.y1;

        if (gdi.y2 < yMin)
            yMin = gdi.y2;

        if (gdi.y1 > yMax)
            yMax = gdi.y1;

        // if (gdi.y2 > yMax)  // We normally don't need this check, because y1 will usually (or always?) be greater than y2.
        //     yMax = gdi.y2;
    }

    const uint16_t destWidth  = (abs(xMin) + xMax);  // abs(xMin) because it's possible the left of the first glyph is to the left of the origin. Note by Paul Pedriana: Shouldn't this instead be (xMax - min(xMin, 0))?
    const uint16_t destHeight = (yMax - yMin);

    if(destWidth && destHeight)  // (If we are drawing just space chars (which often happens), then destWidth and/or destHeight will be zero)
    {
        // Question by Paul Pedriana: What is the following code doing? I copied this from the 
        // WebKit Freetype equivalent of this function assuming it must be useful. It seems to 
        // me that it is chopping off any glyph pixels to the left of zero. This is probably 
        // going to usually be OK, because it is abnormal for the first char of just about any
        // written language to be a combining char (and thus drawn to the left of the pen position).
        if (gdiArray[0].x1 < 0)
        {
            x_offset = gdiArray[0].x1;
            gdiArray[0].x1 = 0;
        }

        EA::Raster::Surface* const pSurface = pGraphicsContext->platformContext();
        const Color                penColor = pGraphicsContext->fillColor();

        // Write the glyphs into a linear buffer.
        // To consider: See if it's possible to often get away with using stack space for this by
        //              using Vector<uint32_t, N> in order to avoid a memory allocation. A typical
        //              width is ~400 pixels and a typical height ~16 pixels, which is 25600 bytes.
        //              So unless the string is small there won't be any benefit to this.
        Vector<uint32_t> glyphRGBABuffer(destWidth * destHeight);
        const int        penA   = penColor.alpha();
        const int        penR   = penColor.red();
        const int        penG   = penColor.green();
        const int        penB   = penColor.blue();
        const int        penRGB = (penR << 16) | (penG << 8) | penB;

        glyphRGBABuffer.fill(0);

		for (int i = 0; i < glyphCount; i++)
        {
			const GlyphDrawInfo& gdi         = gdiArray[i];
            const uint8_t*       pGlyphAlpha = (pTI->GetData()) + (gdi.ty * pTI->GetStride()) + gdi.tx;
            const int            yOffset     = (destHeight + yMin) - gdi.y1;
			// Note by Arpit Baldeva: Old index calculation below caused a memory overrun(or I should say underrun on http://get.adobe.com/flashplayer/).
			// Basically, yOffset * destWidth) + gdi.x1 could end up being negative if the yOffset is 0 and gdi.x1 is negative. Since I do want
			// to make sure that index is positive, I just force it to be at least 0. There is not enough understanding about this code as of now
			// and Paul said that he would take a look at this along with other Font problems he is currently looking at.
			const int bufferIndex = (yOffset * destWidth) + gdi.x1;
			EAW_ASSERT_FORMATTED(bufferIndex>=0, "Buffer Index is negative. This would corrupt memory. yOffset:%d,destWidth:%u,gdi.x1:%d",yOffset,destWidth,gdi.x1);
			uint32_t*            pDestColor  = glyphRGBABuffer.data() + (bufferIndex >= 0 ? bufferIndex : 0);//Old Index calculation- (yOffset * destWidth) + gdi.x1;
            const int            glyphWidth  = (gdi.x2 - gdi.x1);
            const int            glyphHeight = (gdi.y1 - gdi.y2);

			for (int y = 0; y < glyphHeight; ++y)
            {
				for (int x = 0; x < glyphWidth; ++x)
				{
					//+ 1/5/10 CSidhall - When building the text string texture map, kerning can make certain letters overlap and stomp over the alpha of previous letters so 
                    // we need to preserve texels that were aleady set. Should be able to OR herer because we have a clean buffer and penRGB is the same.
                    // Old code:
                    // pDestColor[x] = ((penA * pGlyphAlpha[x] / 255) << 24) | penRGB;
                    // New code:
                    pDestColor[x] |= ((penA * pGlyphAlpha[x] / 255) << 24) | penRGB;
                    // -CS
				}
                
                pDestColor  += destWidth;
                pGlyphAlpha += pTI->GetStride();
            }

        }

        // It would probably be faster if we kept around a surface for multiple usage instead of 
        // continually recreating this one.
        EA::Raster::Surface* const pGlyphSurface = EA::Raster::CreateSurface((void*)glyphRGBABuffer.data(), destWidth, destHeight, destWidth * 4, EA::Raster::kPixelFormatTypeARGB, false, false);
        EA::Raster::Rect rectSrc;
        EA::Raster::Rect rectDest;

        rectSrc.w  = destWidth;
        rectSrc.h  = destHeight;
        rectSrc.x  = 0;
        rectSrc.y  = 0;
        rectDest.x = (int)point.x() + x_offset + pGraphicsContext->origin().width();
        rectDest.y = (int)point.y() - yMax     + pGraphicsContext->origin().height();

        if (pGraphicsContext->transparencyLayer() == 1.0)
            EA::Raster::Blit(pGlyphSurface, &rectSrc, pSurface, &rectDest);
        else
        {
            const float fTransparency = pGraphicsContext->transparencyLayer();
            EA::Raster::Surface* const pSurfaceWithAlpha = EA::Raster::CreateTransparentSurface(pGlyphSurface, (int)(fTransparency * 255));
            EA::Raster::Blit(pSurfaceWithAlpha, &rectSrc, pSurface, &rectDest);
            EA::Raster::DestroySurface(pSurfaceWithAlpha);
        }

        EA::Raster::DestroySurface(pGlyphSurface);
    }
    pTI->DestroyWrapper();

	NOTIFY_PROCESS_STATUS(EA::WebKit::kVProcessTypeDrawGlyph, EA::WebKit::kVProcessStatusEnded);
}


void Font::drawComplexText(GraphicsContext* /*pGraphicsContext*/, const TextRun& /*run*/, const FloatPoint& /*point*/, int /*glyphIndexBegin*/, int /*to*/) const
{
    // To do: implement this. EAText supports Arabic and Hebrew.
}


float Font::floatWidthForComplexText(const TextRun& /*run*/) const
{
    // To do: implement this. EAText supports Arabic and Hebrew.
    return 0.f;
}


int Font::offsetForPositionForComplexText(const TextRun& /*run*/, int /*x*/, bool /*includePartialGlyphs*/) const
{
    // To do: implement this. EAText supports Arabic and Hebrew.
    return 0;
}


FloatRect Font::selectionRectForComplexText(const TextRun& run, const IntPoint& point, int h, int glyphIndexBegin, int to) const
{
    // To do: implement this. EAText supports Arabic and Hebrew.
    return FloatRect();
}


}  // namespace 
