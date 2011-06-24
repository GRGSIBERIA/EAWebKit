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
// EARaster.cpp
// By Paul Pedriana
///////////////////////////////////////////////////////////////////////////////



#include "EARaster.h"
#include "EARasterColor.h"
#include "Color.h"
#include "IntRect.h"
#include <EAWebkit/internal/EAWebKitAssert.h>
#include <math.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>


namespace EA {

namespace Raster {


///////////////////////////////////////////////////////////////////////
// Surface
///////////////////////////////////////////////////////////////////////


Surface::Surface()
{
    InitMembers();
    SetPixelFormat(kPixelFormatTypeARGB);
}


Surface::Surface(int width, int height, PixelFormatType pft)
{
    InitMembers();
    SetPixelFormat(pft);
    Resize(width, height, false);
    // The user can check mpData to see if the resize succeeded.
}


Surface::~Surface()
{
    FreeData();
}


int Surface::AddRef()
{
    // This is not thread-safe.
    return ++mRefCount;
}


int Surface::Release()
{
    // This is not thread-safe.
    if(mRefCount > 1)
        return --mRefCount;

    DestroySurface(this);
    return 0;
}


void Surface::InitMembers()
{
    // mPixelFormat  // Should probably init this.
    mSurfaceFlags  = 0;
    mpData         = NULL;
    mWidth         = 0;
    mHeight        = 0;
    mStride        = 0;
    mLockCount     = 0;
    mRefCount      = 0;
    mpUserData     = NULL;
    mCompressedSize = 0;

    // Draw info.
    mClipRect.x    = 0;
    mClipRect.y    = 0;
    mClipRect.w    = INT_MAX;
    mClipRect.h    = INT_MAX;
    mpBlitDest     = 0;
    mDrawFlags     = 0;
    mpBlitFunction = NULL;
}


void Surface::SetPixelFormat(PixelFormatType pft)
{
    mPixelFormat.mPixelFormatType = pft;
    mPixelFormat.mSurfaceAlpha    = 255;

    if (pft == kPixelFormatTypeRGB)
        mPixelFormat.mBytesPerPixel = 3;
    else
        mPixelFormat.mBytesPerPixel = 4;

    switch (pft)
    {
        case kPixelFormatTypeARGB:
            mPixelFormat.mRMask  = 0x00ff0000;
            mPixelFormat.mGMask  = 0x0000ff00;
            mPixelFormat.mBMask  = 0x000000ff;
            mPixelFormat.mAMask  = 0xff000000;
            mPixelFormat.mRShift = 16;
            mPixelFormat.mGShift = 8;
            mPixelFormat.mBShift = 0;
            mPixelFormat.mAShift = 24;
            break;

        case kPixelFormatTypeRGBA:
            mPixelFormat.mRMask  = 0xff000000;
            mPixelFormat.mGMask  = 0x00ff0000;
            mPixelFormat.mBMask  = 0x0000ff00;
            mPixelFormat.mAMask  = 0x000000ff;
            mPixelFormat.mRShift = 24;
            mPixelFormat.mGShift = 16;
            mPixelFormat.mBShift = 8;
            mPixelFormat.mAShift = 0;
            break;

        case kPixelFornatTypeXRGB:
            mPixelFormat.mRMask  = 0x00ff0000;
            mPixelFormat.mGMask  = 0x0000ff00;
            mPixelFormat.mBMask  = 0x000000ff;
            mPixelFormat.mAMask  = 0x00000000;
            mPixelFormat.mRShift = 16;
            mPixelFormat.mGShift = 8;
            mPixelFormat.mBShift = 0;
            mPixelFormat.mAShift = 24;
            break;

        case kPixelFormatTypeRGBX:
            mPixelFormat.mRMask  = 0xff000000;
            mPixelFormat.mGMask  = 0x00ff0000;
            mPixelFormat.mBMask  = 0x0000ff00;
            mPixelFormat.mAMask  = 0x00000000;
            mPixelFormat.mRShift = 24;
            mPixelFormat.mGShift = 16;
            mPixelFormat.mBShift = 8;
            mPixelFormat.mAShift = 0;
            break;

        case kPixelFormatTypeRGB:
            mPixelFormat.mRMask  = 0xff000000;  // I think this is wrong, or at least endian-dependent.
            mPixelFormat.mGMask  = 0x00ff0000;
            mPixelFormat.mBMask  = 0x0000ff00;
            mPixelFormat.mAMask  = 0x00000000;
            mPixelFormat.mRShift = 24;
            mPixelFormat.mGShift = 16;
            mPixelFormat.mBShift = 8;
            mPixelFormat.mAShift = 0;
            break;
    }
}


bool Surface::Set(void* pData, int width, int height, int stride, PixelFormatType pft, bool bCopyData, bool bTakeOwnership)
{
    FreeData();

    mpBlitDest     = 0;
    mDrawFlags     = 0;
    mpBlitFunction = NULL;

    if(pData)
    {
        SetPixelFormat(pft);

        if(bCopyData)  // If we are copying the data instead of taking over ownership...
        {
            if(Resize(width, height, false))
            {
                // We could blit pSource to ourself, but we happen to know we are of identical
                // format and we simply want to replication source onto ourselves. So memcpy it.
                EAW_ASSERT(mStride == stride); // If this fails then we need to do it a slightly more involved way.
                memcpy(mpData, pData, height * stride);
            }
            else
                return false;
        }
        else
        {
            mpData  = pData;
            mWidth  = width;
            mHeight = height;
            mStride = stride;

            if(!bTakeOwnership)
                mSurfaceFlags |= kFlagOtherOwner;
        }
    }

    return true;
}


bool Surface::Set(Surface* pSource)
{
    if(Resize(pSource->mWidth, pSource->mHeight, false))
    {
        // We could blit pSource to ourself, but we happen to know we are of identical
        // format and we simply want to replication source onto ourselves. So memcpy it.
        EAW_ASSERT(mStride == pSource->mStride); // If this fails then we need to do it a slightly more involved way.
        memcpy(mpData, pSource->mpData, pSource->mHeight * pSource->mStride);

        return true;
    }

    return false;
}


bool Surface::Resize(int width, int height, bool bCopyData)
{
    const size_t kNewMemorySize = width * height * mPixelFormat.mBytesPerPixel;

    if(bCopyData && mpData)
    {
        void* pData = WTF::fastNewArray<char>(kNewMemorySize);

        #ifdef EA_DEBUG
            memset(mpData, 0, kNewMemorySize);
        #endif

        // To do: Copy from mpData to pData.
          EAW_FAIL_MSG("Surface::Resize data copy is not completed.");
        FreeData();
        mpData = pData;
    }
    else
    {
        // We have a separate pathway for this case because it uses less total memory than above.
        FreeData();

        mpData = WTF::fastNewArray<char> (kNewMemorySize);

        #ifdef EA_DEBUG
        if(mpData)    
            memset(mpData, 0, kNewMemorySize);
        #endif
    }

    if(mpData)
    {
        mWidth  = width;
        mHeight = height;
        mStride = width * mPixelFormat.mBytesPerPixel;
    }

    return (mpData != NULL);
}


void Surface::FreeData()
{
    if((mSurfaceFlags & kFlagOtherOwner) == 0)  // If we own the pointer...
    {
        WTF::fastDeleteArray<char> ((char*)mpData);
        mpData = NULL;
    }

    mSurfaceFlags = 0; // Or maybe just mSurfaceFlags &= ~kFlagOtherOwner;
    mWidth        = 0;
    mHeight       = 0;
    mStride       = 0;
    mLockCount    = 0;
}


void Surface::SetClipRect(const Rect* pRect)
{
	Rect fullRect(0, 0, mWidth, mHeight);

	if(pRect)
	    IntersectRect(*pRect, fullRect, mClipRect);
    else
    	mClipRect = fullRect;
}


Surface* CreateSurface()
{
    
    Surface* pSurface = WTF::fastNew<Surface>();
   
    if(pSurface)
        pSurface->AddRef();

    return pSurface;
}


Surface* CreateSurface(int width, int height, PixelFormatType pft)
{
    Surface* pNewSurface = CreateSurface();

    if(pNewSurface)
    {
        // Note that pNewSurface is already AddRef'd.
        pNewSurface->SetPixelFormat(pft);

        if(!pNewSurface->Resize(width, height, false))
        {
            DestroySurface(pNewSurface);
            pNewSurface = NULL;
        }
    }

    return pNewSurface;
}


Surface* CreateSurface(Surface* pSurface)
{
    Surface* pNewSurface = CreateSurface();

    if(pNewSurface)
    {
        // Note that pNewSurface is already AddRef'd.
        if(!pNewSurface->Set(pSurface))
        {
            DestroySurface(pNewSurface);
            pNewSurface = NULL;
        }
    }

    return pNewSurface;
}


Surface* CreateSurface(void* pData, int width, int height, int stride, PixelFormatType pft, bool bCopyData, bool bTakeOwnership)
{
    Surface* pNewSurface = CreateSurface();

    if(pNewSurface)
    {
        // Note that pNewSurface is already AddRef'd.
        if(!pNewSurface->Set(pData, width, height, stride, pft, bCopyData, bTakeOwnership))
        {
            DestroySurface(pNewSurface);
            pNewSurface = NULL;
        }
    }

    return pNewSurface;
}


void DestroySurface(Surface* pSurface)
{
    WTF::fastDelete<Surface>(pSurface);

}






///////////////////////////////////////////////////////////////////////
// Utility functions
///////////////////////////////////////////////////////////////////////

EARASTER_API bool IntersectRect(const Rect& a, const Rect& b, Rect& result)
{
    // Horizontal
    int aMin = a.x;
    int aMax = aMin + a.w;
    int bMin = b.x;
    int bMax = bMin + b.w;

    if(bMin > aMin)
        aMin = bMin;
    result.x = aMin;
    if(bMax < aMax)
        aMax = bMax;
    result.w = (((aMax - aMin) > 0) ? (aMax - aMin) : 0);

    // Vertical
    aMin = a.y;
    aMax = aMin + a.h;
    bMin = b.y;
    bMax = bMin + b.h;

    if(bMin > aMin)
        aMin = bMin;
    result.y = aMin;
    if(bMax < aMax)
        aMax = bMax;
    result.h = (((aMax - aMin) > 0) ? (aMax - aMin) : 0);

    return (result.w && result.h);
}


// To do: Convert this to a .tga writer instead of .ppm, as .tga is more commonly
//        supported yet is still a fairly simple format.
EARASTER_API bool WritePPMFile(const char* pPath, Surface* pSurface, bool bAlphaOnly)
{
    FILE* const fp = fopen(pPath, "w");

    if(fp)
    {
        const bool bARGB = (pSurface->mPixelFormat.mPixelFormatType == EA::Raster::kPixelFormatTypeARGB);

        fprintf(fp, "P3\n");
        fprintf(fp, "# %s\n", pPath);
        fprintf(fp, "%d %d\n", (int)pSurface->mWidth, (int)pSurface->mHeight);
        fprintf(fp, "%d\n", 255);

        for(int y = 0; y < pSurface->mHeight; y++)
        {
            for(int x = 0; x < pSurface->mWidth; x++)
            {
                EA::Raster::Color color; EA::Raster::GetPixel(pSurface, x, y, color);
                const uint32_t    c = color.rgb();
                unsigned          a, r, g, b;

                if(bAlphaOnly)
                {
                    if(bARGB)
                        a = (unsigned)((c >> 24) & 0xff);  // ARGB
                    else
                        a = (unsigned)(c & 0xff);          // RGBA

                    fprintf(fp, "%03u %03u %03u \t", a, a, a);
                }
                else
                {
                    if(bARGB)
                    {
                        r = (unsigned)((c >> 16) & 0xff); // ARGB
                        g = (unsigned)((c >>  8) & 0xff);
                        b = (unsigned)((c >>  0) & 0xff);
                    }
                    else
                    {
                        r = (unsigned)((c >> 24) & 0xff); // RGBA
                        g = (unsigned)((c >> 16) & 0xff);
                        b = (unsigned)((c >>  8) & 0xff);
                    }

                    fprintf(fp, "%03u %03u %03u \t", r, g, b);
                }
            }

            fprintf(fp, "\n");
        }

        fprintf(fp, "\n");
        fclose(fp);

        return true;
    }

    return false;
}


void IntRectToEARect(const WKAL::IntRect& in, EA::Raster::Rect& out)
{
    out = EA::Raster::Rect( in.x(), in.y(), in.width(), in.height() );
}

void EARectToIntRect(const EA::Raster::Rect& in, WKAL::IntRect& out)
{
    out = WebCore::IntRect( in.x, in.y, in.w, in.h );
}

} // namespace Raster

} // namespace EA

namespace EA
{
	namespace Raster
	{
		void EARasterConcrete::IntRectToEARect(const WKAL::IntRect& in, EA::Raster::Rect& out)
		 {
			 EA::Raster::IntRectToEARect(in, out);
		 }
		 void EARasterConcrete::EARectToIntRect(const EA::Raster::Rect& in, WKAL::IntRect& out)
		 {
			EA::Raster::EARectToIntRect(in, out);
		 }
		// Surface management
		 Surface* EARasterConcrete::CreateSurface()
		 {
			 return EA::Raster::CreateSurface();
		 }
		 Surface* EARasterConcrete::CreateSurface(int width, int height, PixelFormatType pft)
		 {
			 return EA::Raster::CreateSurface(width, height, pft);
		 }
		 Surface*    EARasterConcrete::CreateSurface(Surface* pSurface)
		 {
			 return EA::Raster::CreateSurface(pSurface);
		 }
		 Surface* EARasterConcrete::CreateSurface(void* pData, int width, int height, int stride, PixelFormatType pft, bool bCopyData, bool bTakeOwnership)
		 {
			 return EA::Raster::CreateSurface(pData, width, height, stride, pft, bCopyData, bTakeOwnership);
		 }
		 void EARasterConcrete::DestroySurface(Surface* pSurface)
		 {
			 EA::Raster::DestroySurface(pSurface);
		 }

		// Color conversion
		 void EARasterConcrete::ConvertColor(NativeColor c, PixelFormatType cFormat, Color& result)
		 {
			 EA::Raster::ConvertColor(c, cFormat, result);
		 }
		 void EARasterConcrete::ConvertColor(int r, int g, int b, int a, Color& result)
		 {
			 EA::Raster::ConvertColor(r, g, b, a, result);
		 }
		 NativeColor EARasterConcrete::ConvertColor(const Color& c, PixelFormatType resultFormat)
		 {
			 return EA::Raster::ConvertColor(c, resultFormat);
		 }
		 void EARasterConcrete::ConvertColor(const Color& c, int& r, int& g, int& b, int& a)
		 {
			 EA::Raster::ConvertColor(c, r, g, b, a);
		 }
		 void EARasterConcrete::ConvertColor(NativeColor c, const PixelFormat& pf, int& r, int& g, int& b, int& a)
		 {
			 EA::Raster::ConvertColor(c, pf, r, g, b, a);
		 }
		 void EARasterConcrete::ConvertColor(NativeColor c, const PixelFormat& pf, int& r, int& g, int& b)
		 {
			 EA::Raster::ConvertColor(c, pf, r, g, b);
		 }

		// Pixel functions
		 void  EARasterConcrete::GetPixel(Surface* pSurface, int x, int y, Color& color)
		 {
			 EA::Raster::GetPixel(pSurface, x, y, color);
		 }
		 int EARasterConcrete::SetPixelSolidColor(Surface* pSurface, int x, int y, const Color& color)
		 {
			 return EA::Raster::SetPixelSolidColor(pSurface, x, y, color);
		 }
		 int EARasterConcrete::SetPixelSolidColorNoClip(Surface* pSurface, int x, int y, const Color& color)
		 {
			 return EA::Raster::SetPixelSolidColorNoClip(pSurface, x, y, color);
		 }
		 int EARasterConcrete::SetPixelColor(Surface* pSurface, int x, int y, const Color& color)
		 {
			 return EA::Raster::SetPixelColor(pSurface, x, y, color);
		 }
		 int EARasterConcrete::SetPixelColorNoClip(Surface* pSurface, int x, int y, const Color& color)
		 {
			 return EA::Raster::SetPixelColorNoClip(pSurface, x, y, color);
		 }
		 int EARasterConcrete::SetPixelRGBA(Surface* pSurface, int x, int y, int r, int g, int b, int a)
		 {
			 return EA::Raster::SetPixelRGBA(pSurface, x, y, r, g, b, a);
		 }
		 int EARasterConcrete::SetPixelRGBANoClip(Surface* pSurface, int x, int y, int r, int g, int b, int a)
		 {
			 return EA::Raster::SetPixelRGBANoClip(pSurface, x, y, r, g, b, a);
		 }

		// Rectangle functions
		 int EARasterConcrete::FillRectSolidColor(Surface* pSurface, const Rect* pRect, const Color& color)
		 {
			 return EA::Raster::FillRectSolidColor(pSurface, pRect, color);
		 }
		 int EARasterConcrete::FillRectColor(Surface* pSurface, const Rect* pRect, const Color& color)
		 {
			return EA::Raster::FillRectColor(pSurface, pRect, color);
		 }
		 int EARasterConcrete::RectangleColor(Surface* pSurface, int x1, int y1, int x2, int y2, const Color& color)
		 {
			 return EA::Raster::RectangleColor(pSurface, x1, y1, x2, y2, color);
		 }
		 int EARasterConcrete::RectangleColor(Surface* pSurface, const EA::Raster::Rect& rect, const Color& c)
		 {
			 return EA::Raster::RectangleColor(pSurface, rect, c);
		 }
		 int EARasterConcrete::RectangleRGBA(Surface* pSurface, int x1, int y1, int x2, int y2, int r, int g, int b, int a)
		 {
			 return EA::Raster::RectangleRGBA(pSurface, x1, y1, x2, y2, r, g, b, a);
		 }

		// int   BoxColor                (Surface* pSurface, int x1, int y1, int x2, int y2, const Color& color){}
		// int   BoxRGBA                 (Surface* pSurface, int x1, int y1, int x2, int y2, int r, int g, int b, int a){}

		// Line functions
		 int EARasterConcrete::HLineSolidColor(Surface* pSurface, int x1, int x2, int  y, const Color& color)
		 {
			 return EA::Raster::HLineSolidColor(pSurface,x1,x2,y,color);
		 }
		 int EARasterConcrete::HLineSolidRGBA(Surface* pSurface, int x1, int x2, int  y, int r, int g, int b, int a)
		 {
			 return EA::Raster::HLineSolidRGBA(pSurface,  x1,  x2,   y,  r,  g,  b,  a);
		 }
		 int EARasterConcrete::HLineColor(Surface* pSurface, int x1, int x2, int  y, const Color& color)
		 {
			 return EA::Raster::HLineColor(pSurface,  x1,  x2,   y,color);
		 }
		 int EARasterConcrete::HLineRGBA(Surface* pSurface, int x1, int x2, int  y, int r, int g, int b, int a)
		 {
			 return EA::Raster::HLineRGBA(pSurface,  x1,  x2,   y,  r,  g,  b,  a);
		 }
		 int EARasterConcrete::VLineSolidColor(Surface* pSurface, int  x, int y1, int y2, const Color& color)
		 {
			 return EA::Raster::VLineSolidColor(pSurface,   x,  y1,  y2, color);
		 }
		 int EARasterConcrete::VLineSolidRGBA(Surface* pSurface, int  x, int y1, int y2, int r, int g, int b, int a)
		 {
			 return EA::Raster::VLineSolidRGBA(pSurface,   x,  y1,  y2,  r,  g,  b,  a);
		 }
		 int EARasterConcrete::VLineColor(Surface* pSurface, int  x, int y1, int y2, const Color& color)
		 {
			 return EA::Raster::VLineColor(pSurface,   x,  y1,  y2, color);
		 }
		 int EARasterConcrete::VLineRGBA(Surface* pSurface, int  x, int y1, int y2, int r, int g, int b, int a)
		 {
			 return EA::Raster::VLineRGBA(pSurface, x, y1, y2, r, g, b, a);
		 }
		 int EARasterConcrete::LineColor(Surface* pSurface, int x1, int y1, int x2, int y2, const Color& color)
		 {
			 return EA::Raster::LineColor(pSurface, x1, y1, x2, y2, color);
		 }
		 int EARasterConcrete::LineRGBA(Surface* pSurface, int x1, int y1, int x2, int y2, int r, int g, int b, int a)
		 {
			 return EA::Raster::LineRGBA(pSurface,  x1,  y1,  x2,  y2,  r,  g,  b,  a);
		 }
		 int EARasterConcrete::AALineColor(Surface* pSurface, int x1, int y1, int x2, int y2, const Color& color, bool bDrawEndpoint)
		 {
			 return EA::Raster::AALineColor(pSurface,  x1,  y1,  x2,  y2,color,bDrawEndpoint);
		 }
		 int EARasterConcrete::AALineColor(Surface* pSurface, int x1, int y1, int x2, int y2, const Color& color)
		 {
			 return EA::Raster::AALineColor(pSurface,  x1,  y1,  x2,  y2, color);
		 }
		 int EARasterConcrete::AALineRGBA(Surface* pSurface, int x1, int y1, int x2, int y2, int r, int g, int b, int a)
		 {
			 return EA::Raster::AALineRGBA(pSurface,  x1,  y1,  x2,  y2,  r,  g,  b,  a);
		 }

		// Circle / Ellipse
		 int EARasterConcrete::CircleColor(Surface* pSurface, int x, int y, int radius, const Color& color)
		 {
			 return EA::Raster::CircleColor(pSurface,  x,  y,  radius, color);
		 }
		 int EARasterConcrete::CircleRGBA(Surface* pSurface, int x, int y, int radius, int r, int g, int b, int a)
		 {
			 return EA::Raster::CircleRGBA(pSurface,  x,  y,  radius,  r,  g,  b,  a);
		 }
		 int EARasterConcrete::ArcColor(Surface* pSurface, int x, int y, int r, int start, int end, const Color& color)
		 {
			 return EA::Raster::ArcColor(pSurface,  x,  y,  r,  start,  end, color);
		 }
		 int EARasterConcrete::ArcRGBA(Surface* pSurface, int x, int y, int radius, int start, int end, int r, int g, int b, int a)
		 {
			 return EA::Raster::ArcRGBA(pSurface,  x,  y,  radius,  start,  end,  r,  g,  b,  a);
		 }
		 int EARasterConcrete::AACircleColor(Surface* pSurface, int x, int y, int r, const Color& color)
		 {
			 return EA::Raster::AACircleColor(pSurface,  x,  y,  r, color);
		 }
		 int EARasterConcrete::AACircleRGBA(Surface* pSurface, int x, int y, int radius, int r, int g, int b, int a)
		 {
			 return EA::Raster::AACircleRGBA(pSurface,  x,  y,  radius,  r,  g,  b,  a);
		 }
		 int EARasterConcrete::FilledCircleColor(Surface* pSurface, int x, int y, int r, const Color& color)
		 {
			 return EA::Raster::FilledCircleColor(pSurface,  x,  y,  r, color);
		 }
		 int EARasterConcrete::FilledCircleRGBA(Surface* pSurface, int x, int y, int radius, int r, int g, int b, int a)
		 {
			 return EA::Raster::FilledCircleRGBA(pSurface,  x,  y,  radius,  r,  g,  b,  a);
		 }
		 int EARasterConcrete::EllipseColor(Surface* pSurface, int x, int y, int rx, int ry, const Color& color)
		 {
			 return EA::Raster::EllipseColor(pSurface,  x,  y,  rx,  ry,  color);
		 }
		 int EARasterConcrete::EllipseRGBA(Surface* pSurface, int x, int y, int rx, int ry, int r, int g, int b, int a)
		 {
			 return EA::Raster::EllipseRGBA(pSurface,  x,  y,  rx,  ry,  r,  g,  b,  a);
		 }
		 int EARasterConcrete::AAEllipseColor(Surface* pSurface, int xc, int yc, int rx, int ry, const Color& color)
		 {
			 return EA::Raster::AAEllipseColor(pSurface,  xc,  yc,  rx,  ry, color);
		 }
		 int EARasterConcrete::AAEllipseRGBA(Surface* pSurface, int x, int y, int rx, int ry, int r, int g, int b, int a)
		 {
			 return EA::Raster::AAEllipseRGBA(pSurface,  x,  y,  rx,  ry,  r,  g,  b,  a);
		 }
		 int EARasterConcrete::FilledEllipseColor(Surface* pSurface, int x, int y, int rx, int ry, const Color& color)
		 {
			 return EA::Raster::FilledEllipseColor(pSurface,  x,  y,  rx,  ry, color);
		 }
		 int EARasterConcrete::FilledEllipseRGBA(Surface* pSurface, int x, int y, int rx, int ry, int r, int g, int b, int a)
		 {
			 return EA::Raster::FilledEllipseRGBA(pSurface,  x,  y,  rx,  ry,  r,  g,  b,  a);
		 }
		 int EARasterConcrete::PieColor(Surface* pSurface, int x, int y, int radius, int start, int end, const Color& color)
		 {
			 return EA::Raster::PieColor(pSurface,  x,  y,  radius,  start,  end, color);
		 }
		 int EARasterConcrete::PieRGBA(Surface* pSurface, int x, int y, int radius,  int start, int end, int r, int g, int b, int a)
		 {
			 return EA::Raster::PieRGBA(pSurface,  x,  y,  radius,   start,  end,  r,  g,  b,  a);
		 }
		 int EARasterConcrete::FilledPieColor(Surface* pSurface, int x, int y, int radius, int start, int end, const Color& color)
		 {
			 return EA::Raster::FilledPieColor(pSurface,  x,  y,  radius,  start,  end, color);
		 }
		 int EARasterConcrete::FilledPieRGBA(Surface* pSurface, int x, int y, int radius, int start, int end, int r, int g, int b, int a)
		 {
			 return EA::Raster::FilledPieRGBA(pSurface,  x,  y,  radius,  start,  end,  r,  g,  b,  a);
		 }

		// Polygon
		 int EARasterConcrete::SimpleTriangle(Surface* pSurface, int  x, int  y, int size, Orientation o, const Color& color)
		 {
			 return EA::Raster::SimpleTriangle(pSurface,   x,   y,  size, o, color);
		 }
		 int EARasterConcrete::TrigonColor(Surface* pSurface, int x1, int y1, int x2, int y2, int x3, int y3, const Color& color)
		 {
			 return EA::Raster::TrigonColor(pSurface,  x1,  y1,  x2,  y2,  x3,  y3, color);
		 }
		 int EARasterConcrete::TrigonRGBA(Surface* pSurface, int x1, int y1, int x2, int y2, int x3, int y3, int r, int g, int b, int a)
		 {
			 return EA::Raster::TrigonRGBA(pSurface,  x1,  y1,  x2,  y2,  x3,  y3,  r,  g,  b,  a);
		 }
		 int EARasterConcrete::AATrigonColor(Surface* pSurface, int x1, int y1, int x2, int y2, int x3, int y3, const Color& color)
		 {
			 return EA::Raster::AATrigonColor(pSurface,  x1,  y1,  x2,  y2,  x3,  y3, color);
		 }
		 int EARasterConcrete::AATrigonRGBA(Surface* pSurface, int x1, int y1, int x2, int y2, int x3, int y3, int r, int g, int b, int a)
		 {
			 return EA::Raster::AATrigonRGBA(pSurface,  x1,  y1,  x2,  y2,  x3,  y3,  r,  g,  b,  a);
		 }
		 int EARasterConcrete::FilledTrigonColor(Surface* pSurface, int x1, int y1, int x2, int y2, int x3, int y3, const Color& color)
		 {
			 return EA::Raster::FilledTrigonColor(pSurface,  x1,  y1,  x2,  y2,  x3,  y3, color);
		 }
		 int EARasterConcrete::FilledTrigonRGBA(Surface* pSurface, int x1, int y1, int x2, int y2, int x3, int y3, int r, int g, int b, int a)
		 {
			 return EA::Raster::FilledTrigonRGBA(pSurface,  x1,  y1,  x2,  y2,  x3,  y3,  r,  g,  b,  a);
		 }
		 int EARasterConcrete::PolygonColor(Surface* pSurface, const int* vx, const int* vy, int n, const Color& color)
		 {
			 return EA::Raster::PolygonColor(pSurface, vx,  vy,  n, color);
		 }
		 int EARasterConcrete::PolygonRGBA(Surface* pSurface, const int* vx, const int* vy, int n, int r, int g, int b, int a)
		 {
			 return EA::Raster::PolygonRGBA(pSurface, vx, vy,  n,  r,  g,  b,  a);
		 }
		 int EARasterConcrete::AAPolygonColor(Surface* pSurface, const int* vx, const int* vy, int n, const Color& color)
		 {
			 return EA::Raster::AAPolygonColor(pSurface, vx, vy,  n, color);
		 }
		 int EARasterConcrete::AAPolygonRGBA(Surface* pSurface, const int* vx, const int* vy, int n, int r, int g, int b, int a)
		 {
			 return EA::Raster::AAPolygonRGBA(pSurface, vx, vy,  n,  r,  g,  b,  a);
		 }
		 int EARasterConcrete::FilledPolygonColor(Surface* pSurface, const int* vx, const int* vy, int n, const Color& color)
		 {
			 return EA::Raster::FilledPolygonColor(pSurface, vx, vy,  n, color);
		 }
		 int EARasterConcrete::FilledPolygonRGBA(Surface* pSurface, const int* vx, const int* vy, int n, int r, int g, int b, int a)
		 {
			 return EA::Raster::FilledPolygonRGBA(pSurface, vx, vy,  n,  r,  g,  b,  a);
		 }
		 int EARasterConcrete::TexturedPolygon(Surface* pSurface, const int* vx, const int* vy, int n, Surface* pTexture,int texture_dx,int texture_dy)
		 {
			 //notImplemented();
			 return -1;
			 //return EA::Raster::TexturedPolygon(pSurface, vx, vy,  n, pTexture, texture_dx, texture_dy);
		 }
		 int EARasterConcrete::FilledPolygonColorMT(Surface* pSurface, const int* vx, const int* vy, int n, const Color& color, int** polyInts, int* polyAllocated)
		 {
			 return EA::Raster::FilledPolygonColorMT(pSurface, vx,  vy,  n, color, polyInts, polyAllocated);
		 }
		 int EARasterConcrete::FilledPolygonRGBAMT(Surface* pSurface, const int* vx, const int* vy, int n, int r, int g, int b, int a, int** polyInts, int* polyAllocated)
		 {
			 return EA::Raster::FilledPolygonRGBAMT(pSurface, vx, vy,  n,  r,  g,  b,  a, polyInts, polyAllocated);
		 }
		 int EARasterConcrete::TexturedPolygonMT(Surface* pSurface, const int* vx, const int* vy, int n, Surface* pTexture, int texture_dx, int texture_dy, int** polyInts, int* polyAllocated)
		 {
			 //notImplemented();
			 return -1;
			 //return EA::Raster::TexturedPolygonMT(pSurface, vx, vy,  n, pTexture,  texture_dx,  texture_dy, polyInts, polyAllocated);
		 }


		///////////////////////////////////////////////////////////////////////
		// Resampling
		///////////////////////////////////////////////////////////////////////

		 Surface* EARasterConcrete::ZoomSurface(Surface* pSurface, double zoomx, double zoomy, bool bSmooth)
		 {
			 return EA::Raster::ZoomSurface(pSurface, zoomx, zoomy, bSmooth);
		 }

		 void EARasterConcrete::ZoomSurfaceSize(int width, int height, double zoomx, double zoomy, int* dstwidth, int* dstheight)
		 {
			 return EA::Raster::ZoomSurfaceSize(width, height, zoomx, zoomy, dstwidth, dstheight);
		 }

		 Surface* EARasterConcrete::ShrinkSurface(Surface* pSurface, int factorX, int factorY)
		 {
			 return EA::Raster::ShrinkSurface(pSurface, factorX, factorY);
		 }

		 Surface* EARasterConcrete::RotateSurface90Degrees(Surface* pSurface, int nClockwiseTurns)
		 {
			 return EA::Raster::RotateSurface90Degrees(pSurface, nClockwiseTurns);
		 }

		 Surface* EARasterConcrete::CreateTransparentSurface(Surface* pSource, int surfaceAlpha)
		 {
			 return EA::Raster::CreateTransparentSurface(pSource, surfaceAlpha);
		 }


		///////////////////////////////////////////////////////////////////////
		// Blit functions
		///////////////////////////////////////////////////////////////////////

		 bool EARasterConcrete::ClipForBlit(Surface* pSource, const Rect* pRectSource, Surface* pDest, const Rect* pRectDest, Rect& rectSourceResult, Rect& rectDestResult)
		 {
			 return EA::Raster::ClipForBlit(pSource, pRectSource, pDest, pRectDest, rectSourceResult, rectDestResult);
		 }
		 int EARasterConcrete::Blit(Surface* pSource, const Rect* pRectSource, Surface* pDest, const Rect* pRectDest, const Rect* pDestClipRect, const bool additiveBlend)
		 {
			 return EA::Raster::Blit(pSource, pRectSource, pDest, pRectDest, pDestClipRect, additiveBlend);
		 }

		 int EARasterConcrete::BlitNoClip(Surface* pSource, const Rect* pRectSource, Surface* pDest, const Rect* pRectDest, const bool additiveBlend)
		 {
			 return EA::Raster::BlitNoClip(pSource, pRectSource, pDest, pRectDest, additiveBlend);
		 }

		 int EARasterConcrete::BlitTiled(Surface* pSource, const Rect* pRectSource, Surface* pDest, const Rect* pRectDest, int offsetX, int offsetY)
		 {
			 return EA::Raster::BlitTiled(pSource, pRectSource, pDest, pRectDest, offsetX, offsetY);
		 }

		 int EARasterConcrete::BlitEdgeTiled(Surface* pSource, const Rect* pRectSource, Surface* pDest, const Rect* pRectDest, const Rect* pRectSourceCenter)
		 {
			 return EA::Raster::BlitEdgeTiled(pSource, pRectSource, pDest, pRectDest, pRectSourceCenter);
		 }

		 bool EARasterConcrete::SetupBlitFunction(Surface* pSource, Surface* pDest)
		 {
			 return EA::Raster::SetupBlitFunction(pSource, pDest);
		 }

		///////////////////////////////////////////////////////////////////////
		// Utility functions
		///////////////////////////////////////////////////////////////////////

		// The result parameter may refer to one of the source parameters.
		 bool EARasterConcrete::IntersectRect(const Rect& a, const Rect& b, Rect& result)
		 {
			 return EA::Raster::IntersectRect(a, b, result);
		 }

		// A PPM file is a simple bitmap format which many picture viewers can read.
		 bool EARasterConcrete::WritePPMFile(const char* pPath, Surface* pSurface, bool bAlphaOnly)
		 {
			 return EA::Raster::WritePPMFile(pPath, pSurface, bAlphaOnly);
		 }

		 RGBA32 EARasterConcrete::makeRGB(int32_t r, int32_t g, int32_t b)
		 {
			 return EA::Raster::makeRGB( r,  g,  b);
		 }
		 RGBA32 EARasterConcrete::makeRGBA(int32_t r, int32_t g, int32_t b, int32_t a)
		 {
			 return EA::Raster::makeRGBA( r,  g,  b,  a);
		 }

	}
}

