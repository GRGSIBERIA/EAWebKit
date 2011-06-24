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
// EARaster.h
// By Paul Pedriana
///////////////////////////////////////////////////////////////////////////////


#ifndef EARASTER_EARASTER_H
#define EARASTER_EARASTER_H


#include <EABase/eabase.h>
#include <EARaster/EARasterColor.h>
#include <EARaster/EARasterConfig.h>


namespace WKAL  // a.k.a. namespace WebCore
{
    class IntRect;
}


namespace EA
{
    namespace Raster
    {
        // Forward declarations
        class Surface;
        class Color;


        // Typedefs
        typedef uint32_t NativeColor;     // Refers to a color used by a surface in the surface's PixelFormatType.


        // Orientation
        // These can be used as values or as flags.
        enum Orientation
        {
            kOLeft  = 0x01,
            kOUp    = 0x02,     // a.k.a. Top
            kORight = 0x04,
            kODown  = 0x08      // a.k.a. Bottom
        };


        // Note: WebKit's WKAL::Color type declares itself to be RGBA, but it is wrong; 
        // it is using ARGB, in particular the 32 bit 0xAARRGGBB type as we have below.
        enum PixelFormatType
        {
            kPixelFormatTypeARGB,       // 32 bit 0xAARRGGBB, byte order in memory depends on endian-ness. For little endian it is B, G, R, A; for big endian it is A, R, G, B.
            kPixelFormatTypeRGBA,       // 32 bit 0xRRGGBBAA, byte order in memory depends on endian-ness. For little endian it is A, B, G, R; for big endian it is R, G, B, A. 
            kPixelFornatTypeXRGB,       // 32 bit 0xXXRRGGBB, byte order in memory depends on endian-ness. The X means the data is unused or can be considered to be always 0xff.
            kPixelFormatTypeRGBX,       // 32 bit 0xRRGGBBXX, byte order in memory depends on endian-ness. The X means the data is unused or can be considered to be always 0xff.
            kPixelFormatTypeRGB         // 24 bit 0xRRGGBB, byte order in memory is always R, G, B.
        };


        struct PixelFormat
        {
            PixelFormatType mPixelFormatType;   // e.g. kPixelFormatTypeARGB.
            uint8_t         mBytesPerPixel;     // Typically four, as in the case of ARGB.
            uint8_t         mSurfaceAlpha;      // Alpha that is applied to the entire surface and not just per pixel.
	        uint32_t        mRMask;
	        uint32_t        mGMask;
	        uint32_t        mBMask;
	        uint32_t        mAMask;
	        uint8_t         mRShift;
	        uint8_t         mGShift;
	        uint8_t         mBShift;
	        uint8_t         mAShift;
        };


        enum SurfaceFlags
        {
            kFlagRAMSurface     = 0x00,     // The pixel data resides in regular RAM.
            kFlagTextureSurface = 0x01,     // The pixel data resides in a hardware texture instead of RAM.
            kFlagOtherOwner     = 0x02,     // The Surface data is owned by somebody other than the Surface, and the Surface doesn't free it.
            kFlagDisableAlpha   = 0x04,     // Source alpha blending is used if the source has an alpha channel.
            kFlagIgnoreCompressRLE         = 0x08, // This image did not compress so ignore it for compression -CS Added 1/ 15/09
            kFlagIgnoreCompressYCOCGDXT5   = 0x10, // This image did not compress so ignore it for duplicate compression
            kFlagCompressedRLE             = 0x20, // Set when image was compressed using RLE
            kFlagCompressedYCOCGDXT5       = 0x40  // Set when image was compressed using RLE
        };


        struct BlitInfo
        {
            Surface* mpSource;
            uint8_t* mpSPixels;     // Start of blit data.
            int      mnSWidth;      // Width of blit data.
            int      mnSHeight;     // Height of blit data.
            int      mnSSkip;       // Bytes from end of blit row to beginning of new blit row.

            Surface* mpDest;
            uint8_t* mpDPixels;
            int      mnDWidth;
            int      mnDHeight;
            int      mnDSkip;
            bool     mDoAdditiveBlend; // If true, will do additive alpha blending instead of normal alpha blending
        };

        typedef void (*BlitFunctionType)(const BlitInfo& blitInfo);


        enum DrawFlags
        {
            kDrawFlagIdenticalFormats = 0x01    // The source and dest surfaces of a surface to surface operation are of identical format.
        };

        struct Point
        {
            int x;
            int y;

            Point() { } // By design, don't initialize members.
            Point(int px, int py) : x(px), y(py) { }

            void set(int px, int py) {x = px; y = py;}
        };


        // For now we use a x/y/w/h rect instead of a x0/y0/x1/y1 rect like often seen.
        // This is because WebKit's IntRect class works like that. The problem with 
        // the IntRect class is that it is tedious to use.
        struct Rect
        {
            int x;
            int y;
            int w;
            int h;

            Rect() { }  // By design, don't initialize members.

            Rect(int xNew, int yNew, int wNew, int hNew) 
                : x(xNew), y(yNew), w(wNew), h(hNew) { }

            int width()  const { return w; }
            int height() const { return h; }

            bool isPointInside(const Point& p) const { return (p.x >= x) && (p.x <= x + w) && (p.y >= y) && (p.y <= y + h); }

            void constrainRect(Rect& r) const
            {
                //if r is outside of this rect, set width or height to zero
                if(r.x > x+w || r.x + r.w < x)
                {
                    r.w = 0;
                    return;
                }
                if(r.y > y+h || r.y + r.h < y)
                {
                    r.h = 0;
                    return;
                }

                if(r.x < x)
                {
                    r.w -= x - r.x;
                    r.x = x;
                }

                if(r.x + r.w > x + w)
                {
                    r.w -= (r.x + r.w) - (x + w);
                }

                if(r.y < y)
                {
                    r.h -= y - r.y;
                    r.y = y;
                }

                if(r.y + r.h > y + h)
                {
                    r.h -= (r.y + r.h) - (y + h);
                }
            }
        };

        EARASTER_API void IntRectToEARect(const WKAL::IntRect& in, EA::Raster::Rect& out);
        EARASTER_API void EARectToIntRect(const EA::Raster::Rect& in, WKAL::IntRect& out);


        // Surface
        //
        // This class implements a single rectangular 2D pixel surface. 
        //
        class EARASTER_API Surface
        {
        public:
            Surface();
            Surface(int width, int height, PixelFormatType pft);
           ~Surface();

            int  AddRef();
            int  Release();

            void SetPixelFormat(PixelFormatType pft);
            bool Set(void* pData, int width, int height, int stride, PixelFormatType pft, bool bCopyData, bool bTakeOwnership);
            bool Set(Surface* pSource);
            bool Resize(int width, int height, bool bCopyData);
            void FreeData();
            void SetClipRect(const Rect* pRect);

        protected:
            void InitMembers();

        public:
            PixelFormat         mPixelFormat;   // The format of the pixel data.
            int                 mSurfaceFlags;  // See enum SurfaceFlags.
            void*               mpData;         // The pixel data itself.
            int                 mWidth;         // In pixels.
            int                 mHeight;        // In pixels.
            int                 mStride;        // In bytes.
            int                 mLockCount;     // Used if the surface is somewhere other than conventional RAM.
            int                 mRefCount;      // Factory functions return a Surface with a mRefCount of 1.
            void*               mpUserData;     // Arbitrary data the user can associate with this surface.
            int                 mCompressedSize; // Size of buffer if compression was used - CS 1/15/09 Added
            // Draw info.
            Rect                mClipRect;      // Drawing is restricted to within this rect.
            Surface*            mpBlitDest;     // The last surface blitted to. Allows us to cache blit calculations.
            int                 mDrawFlags;     // See enum DrawFlags.
            BlitFunctionType    mpBlitFunction; // The blitting function currently used to blit to mpBlitDest.
        };


        ///////////////////////////////////////////////////////////////////////
        // Primitive functions
        //
        // Functions with int return values use 0 for OK and -1 for error.
        //
        // Coordinates are end-inclusive. Thus a line drawn from x1 to x2 includes
        // a point at x2.
        //
        // Note the use of NativeColor vs. Color. NativeColor is a uint32_t
        // which is always in the pixel format of the surface that is using
        // it. This is as opposed to the Color type which is a generic 
        // ARGB color type which may need to be converted to a surface's
        // native type before written to the surface.
        ///////////////////////////////////////////////////////////////////////

        // Surface management
        EARASTER_API Surface*    CreateSurface();
        EARASTER_API Surface*    CreateSurface(int width, int height, PixelFormatType pft);
        EARASTER_API Surface*    CreateSurface(Surface* pSurface);
        EARASTER_API Surface*    CreateSurface(void* pData, int width, int height, int stride, PixelFormatType pft, bool bCopyData, bool bTakeOwnership);
        EARASTER_API void        DestroySurface(Surface* pSurface);

        // Color conversion
        EARASTER_API void        ConvertColor(NativeColor c, PixelFormatType cFormat, Color& result);
        EARASTER_API void        ConvertColor(int r, int g, int b, int a, Color& result);
        EARASTER_API NativeColor ConvertColor(const Color& c, PixelFormatType resultFormat);
        EARASTER_API void        ConvertColor(const Color& c, int& r, int& g, int& b, int& a);
        EARASTER_API void        ConvertColor(NativeColor c, const PixelFormat& pf, int& r, int& g, int& b, int& a);
        EARASTER_API void        ConvertColor(NativeColor c, const PixelFormat& pf, int& r, int& g, int& b);

        // Pixel functions
        EARASTER_API void  GetPixel                (Surface* pSurface, int x, int y, Color& color);
        EARASTER_API int   SetPixelSolidColor      (Surface* pSurface, int x, int y, const Color& color);
        EARASTER_API int   SetPixelSolidColorNoClip(Surface* pSurface, int x, int y, const Color& color);
        EARASTER_API int   SetPixelColor           (Surface* pSurface, int x, int y, const Color& color);
        EARASTER_API int   SetPixelColorNoClip     (Surface* pSurface, int x, int y, const Color& color);
        EARASTER_API int   SetPixelRGBA            (Surface* pSurface, int x, int y, int r, int g, int b, int a);
        EARASTER_API int   SetPixelRGBANoClip      (Surface* pSurface, int x, int y, int r, int g, int b, int a);

        // Rectangle functions
        EARASTER_API int   FillRectSolidColor      (Surface* pSurface, const Rect* pRect, const Color& color);
        EARASTER_API int   FillRectColor           (Surface* pSurface, const Rect* pRect, const Color& color);
        EARASTER_API int   RectangleColor          (Surface* pSurface, int x1, int y1, int x2, int y2, const Color& color);
        EARASTER_API int   RectangleColor          (Surface* pSurface, const EA::Raster::Rect& rect, const Color& c);
        EARASTER_API int   RectangleRGBA           (Surface* pSurface, int x1, int y1, int x2, int y2, int r, int g, int b, int a);

      //EARASTER_API int   BoxColor                (Surface* pSurface, int x1, int y1, int x2, int y2, const Color& color);
      //EARASTER_API int   BoxRGBA                 (Surface* pSurface, int x1, int y1, int x2, int y2, int r, int g, int b, int a);

        // Line functions
        EARASTER_API int   HLineSolidColor(Surface* pSurface, int x1, int x2, int  y, const Color& color);
        EARASTER_API int   HLineSolidRGBA (Surface* pSurface, int x1, int x2, int  y, int r, int g, int b, int a);
        EARASTER_API int   HLineColor     (Surface* pSurface, int x1, int x2, int  y, const Color& color);
        EARASTER_API int   HLineRGBA      (Surface* pSurface, int x1, int x2, int  y, int r, int g, int b, int a);
        EARASTER_API int   VLineSolidColor(Surface* pSurface, int  x, int y1, int y2, const Color& color);
        EARASTER_API int   VLineSolidRGBA (Surface* pSurface, int  x, int y1, int y2, int r, int g, int b, int a);
        EARASTER_API int   VLineColor     (Surface* pSurface, int  x, int y1, int y2, const Color& color);
        EARASTER_API int   VLineRGBA      (Surface* pSurface, int  x, int y1, int y2, int r, int g, int b, int a);
        EARASTER_API int   LineColor      (Surface* pSurface, int x1, int y1, int x2, int y2, const Color& color);
        EARASTER_API int   LineRGBA       (Surface* pSurface, int x1, int y1, int x2, int y2, int r, int g, int b, int a);
        EARASTER_API int   AALineColor    (Surface* pSurface, int x1, int y1, int x2, int y2, const Color& color, bool bDrawEndpoint);
        EARASTER_API int   AALineColor    (Surface* pSurface, int x1, int y1, int x2, int y2, const Color& color);
        EARASTER_API int   AALineRGBA     (Surface* pSurface, int x1, int y1, int x2, int y2, int r, int g, int b, int a);

        // Circle / Ellipse
        EARASTER_API int   CircleColor       (Surface* pSurface, int x, int y, int radius, const Color& color);
        EARASTER_API int   CircleRGBA        (Surface* pSurface, int x, int y, int radius, int r, int g, int b, int a);
        EARASTER_API int   ArcColor          (Surface* pSurface, int x, int y, int r, int start, int end, const Color& color);
        EARASTER_API int   ArcRGBA           (Surface* pSurface, int x, int y, int radius, int start, int end, int r, int g, int b, int a);
        EARASTER_API int   AACircleColor     (Surface* pSurface, int x, int y, int r, const Color& color);
        EARASTER_API int   AACircleRGBA      (Surface* pSurface, int x, int y, int radius, int r, int g, int b, int a);
        EARASTER_API int   FilledCircleColor (Surface* pSurface, int x, int y, int r, const Color& color);
        EARASTER_API int   FilledCircleRGBA  (Surface* pSurface, int x, int y, int radius, int r, int g, int b, int a);
        EARASTER_API int   EllipseColor      (Surface* pSurface, int x, int y, int rx, int ry, const Color& color);
        EARASTER_API int   EllipseRGBA       (Surface* pSurface, int x, int y, int rx, int ry, int r, int g, int b, int a);
        EARASTER_API int   AAEllipseColor    (Surface* pSurface, int xc, int yc, int rx, int ry, const Color& color);
        EARASTER_API int   AAEllipseRGBA     (Surface* pSurface, int x, int y, int rx, int ry, int r, int g, int b, int a);
        EARASTER_API int   FilledEllipseColor(Surface* pSurface, int x, int y, int rx, int ry, const Color& color);
        EARASTER_API int   FilledEllipseRGBA (Surface* pSurface, int x, int y, int rx, int ry, int r, int g, int b, int a);
        EARASTER_API int   PieColor          (Surface* pSurface, int x, int y, int radius, int start, int end, const Color& color);
        EARASTER_API int   PieRGBA           (Surface* pSurface, int x, int y, int radius,  int start, int end, int r, int g, int b, int a);
        EARASTER_API int   FilledPieColor    (Surface* pSurface, int x, int y, int radius, int start, int end, const Color& color);
        EARASTER_API int   FilledPieRGBA     (Surface* pSurface, int x, int y, int radius, int start, int end, int r, int g, int b, int a);

        // Polygon
        EARASTER_API int   SimpleTriangle      (Surface* pSurface, int  x, int  y, int size, Orientation o, const Color& color);
        EARASTER_API int   TrigonColor         (Surface* pSurface, int x1, int y1, int x2, int y2, int x3, int y3, const Color& color);
        EARASTER_API int   TrigonRGBA          (Surface* pSurface, int x1, int y1, int x2, int y2, int x3, int y3, int r, int g, int b, int a);
        EARASTER_API int   AATrigonColor       (Surface* pSurface, int x1, int y1, int x2, int y2, int x3, int y3, const Color& color);
        EARASTER_API int   AATrigonRGBA        (Surface* pSurface, int x1, int y1, int x2, int y2, int x3, int y3, int r, int g, int b, int a);
        EARASTER_API int   FilledTrigonColor   (Surface* pSurface, int x1, int y1, int x2, int y2, int x3, int y3, const Color& color);
        EARASTER_API int   FilledTrigonRGBA    (Surface* pSurface, int x1, int y1, int x2, int y2, int x3, int y3, int r, int g, int b, int a);
        EARASTER_API int   PolygonColor        (Surface* pSurface, const int* vx, const int* vy, int n, const Color& color);
        EARASTER_API int   PolygonRGBA         (Surface* pSurface, const int* vx, const int* vy, int n, int r, int g, int b, int a);
        EARASTER_API int   AAPolygonColor      (Surface* pSurface, const int* vx, const int* vy, int n, const Color& color);
        EARASTER_API int   AAPolygonRGBA       (Surface* pSurface, const int* vx, const int* vy, int n, int r, int g, int b, int a);
        EARASTER_API int   FilledPolygonColor  (Surface* pSurface, const int* vx, const int* vy, int n, const Color& color);
        EARASTER_API int   FilledPolygonRGBA   (Surface* pSurface, const int* vx, const int* vy, int n, int r, int g, int b, int a);
        EARASTER_API int   TexturedPolygon     (Surface* pSurface, const int* vx, const int* vy, int n, Surface* pTexture,int texture_dx,int texture_dy);
        EARASTER_API int   FilledPolygonColorMT(Surface* pSurface, const int* vx, const int* vy, int n, const Color& color, int** polyInts, int* polyAllocated);
        EARASTER_API int   FilledPolygonRGBAMT (Surface* pSurface, const int* vx, const int* vy, int n, int r, int g, int b, int a, int** polyInts, int* polyAllocated);
        EARASTER_API int   TexturedPolygonMT   (Surface* pSurface, const int* vx, const int* vy, int n, Surface* pTexture, int texture_dx, int texture_dy, int** polyInts, int* polyAllocated);


        ///////////////////////////////////////////////////////////////////////
        // Resampling
        ///////////////////////////////////////////////////////////////////////

        // Zoom
        // Zooms a 32bit or 8bit 'src' surface to newly created 'dst' surface.
        // 'zoomx' and 'zoomy' are scaling factors for width and height. If 'smooth' is 1
        // then the destination 32bit surface is anti-aliased. If the surface is not 8bit
        // or 32bit RGBA/ABGR it will be converted into a 32bit RGBA format on the fly.
        // zoomX/zoomY can be less than 1.0 for shrinking.
        EARASTER_API Surface* ZoomSurface(Surface* pSurface, double zoomx, double zoomy, bool bSmooth);

        // Returns the size of the target surface for a zoomSurface() call
        EARASTER_API void ZoomSurfaceSize(int width, int height, double zoomx, double zoomy, int* dstwidth, int* dstheight);

        // Shrinks a 32bit or 8bit surface to a newly created surface.
        // 'factorX' and 'factorY' are the shrinking ratios (i.e. 2 = 1/2 the size,
        // 3 = 1/3 the size, etc.) The destination surface is antialiased by averaging
        // the source box RGBA or Y information. If the surface is not 8bit
        // or 32bit RGBA/ABGR it will be converted into a 32bit RGBA format on the fly.
        EARASTER_API Surface* ShrinkSurface(Surface* pSurface, int factorX, int factorY);

        // Returns a rotated surface by 90, 180, or 270 degrees.
        EARASTER_API Surface* RotateSurface90Degrees(Surface* pSurface, int nClockwiseTurns);

        // Creates a surface that is the same as pSource but with surfaceAlpha multiplied into pSource.
        EARASTER_API Surface* CreateTransparentSurface(Surface* pSource, int surfaceAlpha);


        ///////////////////////////////////////////////////////////////////////
        // Blit functions
        ///////////////////////////////////////////////////////////////////////

        // Generates rectSourceResult and rectDestResult from source and dest
        // surfaces and unclipped rectangles.
        EARASTER_API bool ClipForBlit(Surface* pSource, const Rect* pRectSource, Surface* pDest, const Rect* pRectDest, Rect& rectSourceResult, Rect& rectDestResult);

        // Does a 1:1 blit from pRectSource in pSource to pRectDest in pDest. 
        // Handles the case whereby pRectSource and pRectDest may refer to out of bounds of 
        // pSource and pDest, respectively.
        // If pDestClipRect is non-NULL, the output is further clipped to pDestClipRect.
        // pDestClipRect is not quite the same as pRectDest, as it's sometimes 
        // useful to blit a source rect to a dest rect but have it clip to another rect.
        // Returns 0 if OK or a negative error code.
        EARASTER_API int Blit(Surface* pSource, const Rect* pRectSource, Surface* pDest, const Rect* pRectDest, const Rect* pDestClipRect = NULL, const bool additiveBlend = false);

        // Does a 1:1 blit from pSource to pDest with the assumption that pRectSource and 
        // pRectDest are already clipped to pSource and pDest, respectively.
        // Returns 0 if OK or a negative error code.
        EARASTER_API int BlitNoClip(Surface* pSource, const Rect* pRectSource, Surface* pDest, const Rect* pRectDest, const bool additiveBlend = false);

        //////////////////////////////////////////////////////////////////////////
        /// Blit a repeating pattern.
        /// The offsetX/Y position is the location within pRectDest that 
        /// the source origin will be. The blit is clipped to within pRectDest.
        /// If pRectSource is NULL then the entire Source is used, else the part
        /// of pSource that is tiled into pDest is the defined by pRectSource.
        EARASTER_API int BlitTiled(Surface* pSource, const Rect* pRectSource, Surface* pDest, const Rect* pRectDest, int offsetX, int offsetY);

        //////////////////////////////////////////////////////////////////////////
        /// This function stretches an image over a rectangular region
        /// by repeating (tiling) the center portion of the image.
        /// This is useful for drawing arbitrarily sized GUI buttons and
        /// scrollbar parts.
        ///
        /// The image is divided like so:
        /// +-----------+-----------+-----------+
        /// | TL Corner |   Top     | TR Corner |
        /// |           |   Edge    |           |
        /// +-----------+-----------+-----------+
        /// | Left      |   Center  | Right     |
        /// | Edge      |           | Edge      |
        /// +-----------+-----------+-----------+
        /// | BL Corner |   Bottom  | BR Corner |
        /// |           |   Edge    |           |
        /// +-----------+-----------+-----------+
        /// The center portions will be tiled to cover the destination
        /// rectangle, where the edge portions will only be blitted
        /// once.
        ///
        /// Parameters:
        ///   pDest - the target drawing context.
        ///   pRectDest - the destination rectangle to fill
        ///   pImage - the source image to render.
        ///   pRectSource - the source rectangle which has nine parts.
        ///   pRectSourceCenter - Defines where the dividing lines between center
        ///       and edges are (For example, mLeft controls the position
        ///       of the dividing line between left edge and center.)
        EARASTER_API int BlitEdgeTiled(Surface* pSource, const Rect* pRectSource, Surface* pDest, const Rect* pRectDest, const Rect* pRectSourceCenter);

        // Sets up the blit function needed to blit pSource to pDest.
        // Normally you don't need to call this function, as the Surface class and Blit 
        // functions will do it automatically.
        EARASTER_API bool SetupBlitFunction(Surface* pSource, Surface* pDest);



        ///////////////////////////////////////////////////////////////////////
        // Utility functions
        ///////////////////////////////////////////////////////////////////////

        // The result parameter may refer to one of the source parameters.
        EARASTER_API bool IntersectRect(const Rect& a, const Rect& b, Rect& result);

        // A PPM file is a simple bitmap format which many picture viewers can read.
        EARASTER_API bool WritePPMFile(const char* pPath, Surface* pSurface, bool bAlphaOnly);

		class IEARaster
		{
		public:
			virtual ~IEARaster()
			{

			}
			virtual void IntRectToEARect(const WKAL::IntRect& in, EA::Raster::Rect& out) = 0;
			virtual void EARectToIntRect(const EA::Raster::Rect& in, WKAL::IntRect& out) = 0;
			// Surface management
			virtual Surface*    CreateSurface() = 0;
			virtual Surface*    CreateSurface(int width, int height, PixelFormatType pft) = 0;
			virtual Surface*    CreateSurface(Surface* pSurface) = 0;
			virtual Surface*    CreateSurface(void* pData, int width, int height, int stride, PixelFormatType pft, bool bCopyData, bool bTakeOwnership) = 0;
			virtual void        DestroySurface(Surface* pSurface) = 0;

			// Color conversion
			virtual void        ConvertColor(NativeColor c, PixelFormatType cFormat, Color& result) = 0;
			virtual void        ConvertColor(int r, int g, int b, int a, Color& result) = 0;
			virtual NativeColor ConvertColor(const Color& c, PixelFormatType resultFormat) = 0;
			virtual void        ConvertColor(const Color& c, int& r, int& g, int& b, int& a) = 0;
			virtual void        ConvertColor(NativeColor c, const PixelFormat& pf, int& r, int& g, int& b, int& a) = 0;
			virtual void        ConvertColor(NativeColor c, const PixelFormat& pf, int& r, int& g, int& b) = 0;

			// Pixel functions
			virtual void  GetPixel                (Surface* pSurface, int x, int y, Color& color) = 0;
			virtual int   SetPixelSolidColor      (Surface* pSurface, int x, int y, const Color& color) = 0;
			virtual int   SetPixelSolidColorNoClip(Surface* pSurface, int x, int y, const Color& color) = 0;
			virtual int   SetPixelColor           (Surface* pSurface, int x, int y, const Color& color) = 0;
			virtual int   SetPixelColorNoClip     (Surface* pSurface, int x, int y, const Color& color) = 0;
			virtual int   SetPixelRGBA            (Surface* pSurface, int x, int y, int r, int g, int b, int a) = 0;
			virtual int   SetPixelRGBANoClip      (Surface* pSurface, int x, int y, int r, int g, int b, int a) = 0;

			// Rectangle functions
			virtual int   FillRectSolidColor      (Surface* pSurface, const Rect* pRect, const Color& color) = 0;
			virtual int   FillRectColor           (Surface* pSurface, const Rect* pRect, const Color& color) = 0;
			virtual int   RectangleColor          (Surface* pSurface, int x1, int y1, int x2, int y2, const Color& color) = 0;
			virtual int   RectangleColor          (Surface* pSurface, const EA::Raster::Rect& rect, const Color& c) = 0;
			virtual int   RectangleRGBA           (Surface* pSurface, int x1, int y1, int x2, int y2, int r, int g, int b, int a) = 0;

			//virtual int   BoxColor                (Surface* pSurface, int x1, int y1, int x2, int y2, const Color& color) = 0;
			//virtual int   BoxRGBA                 (Surface* pSurface, int x1, int y1, int x2, int y2, int r, int g, int b, int a) = 0;

			// Line functions
			virtual int   HLineSolidColor(Surface* pSurface, int x1, int x2, int  y, const Color& color) = 0;
			virtual int   HLineSolidRGBA (Surface* pSurface, int x1, int x2, int  y, int r, int g, int b, int a) = 0;
			virtual int   HLineColor     (Surface* pSurface, int x1, int x2, int  y, const Color& color) = 0;
			virtual int   HLineRGBA      (Surface* pSurface, int x1, int x2, int  y, int r, int g, int b, int a) = 0;
			virtual int   VLineSolidColor(Surface* pSurface, int  x, int y1, int y2, const Color& color) = 0;
			virtual int   VLineSolidRGBA (Surface* pSurface, int  x, int y1, int y2, int r, int g, int b, int a) = 0;
			virtual int   VLineColor     (Surface* pSurface, int  x, int y1, int y2, const Color& color) = 0;
			virtual int   VLineRGBA      (Surface* pSurface, int  x, int y1, int y2, int r, int g, int b, int a) = 0;
			virtual int   LineColor      (Surface* pSurface, int x1, int y1, int x2, int y2, const Color& color) = 0;
			virtual int   LineRGBA       (Surface* pSurface, int x1, int y1, int x2, int y2, int r, int g, int b, int a) = 0;
			virtual int   AALineColor    (Surface* pSurface, int x1, int y1, int x2, int y2, const Color& color, bool bDrawEndpoint) = 0;
			virtual int   AALineColor    (Surface* pSurface, int x1, int y1, int x2, int y2, const Color& color) = 0;
			virtual int   AALineRGBA     (Surface* pSurface, int x1, int y1, int x2, int y2, int r, int g, int b, int a) = 0;

			// Circle / Ellipse
			virtual int   CircleColor       (Surface* pSurface, int x, int y, int radius, const Color& color) = 0;
			virtual int   CircleRGBA        (Surface* pSurface, int x, int y, int radius, int r, int g, int b, int a) = 0;
			virtual int   ArcColor          (Surface* pSurface, int x, int y, int r, int start, int end, const Color& color) = 0;
			virtual int   ArcRGBA           (Surface* pSurface, int x, int y, int radius, int start, int end, int r, int g, int b, int a) = 0;
			virtual int   AACircleColor     (Surface* pSurface, int x, int y, int r, const Color& color) = 0;
			virtual int   AACircleRGBA      (Surface* pSurface, int x, int y, int radius, int r, int g, int b, int a) = 0;
			virtual int   FilledCircleColor (Surface* pSurface, int x, int y, int r, const Color& color) = 0;
			virtual int   FilledCircleRGBA  (Surface* pSurface, int x, int y, int radius, int r, int g, int b, int a) = 0;
			virtual int   EllipseColor      (Surface* pSurface, int x, int y, int rx, int ry, const Color& color) = 0;
			virtual int   EllipseRGBA       (Surface* pSurface, int x, int y, int rx, int ry, int r, int g, int b, int a) = 0;
			virtual int   AAEllipseColor    (Surface* pSurface, int xc, int yc, int rx, int ry, const Color& color) = 0;
			virtual int   AAEllipseRGBA     (Surface* pSurface, int x, int y, int rx, int ry, int r, int g, int b, int a) = 0;
			virtual int   FilledEllipseColor(Surface* pSurface, int x, int y, int rx, int ry, const Color& color) = 0;
			virtual int   FilledEllipseRGBA (Surface* pSurface, int x, int y, int rx, int ry, int r, int g, int b, int a) = 0;
			virtual int   PieColor          (Surface* pSurface, int x, int y, int radius, int start, int end, const Color& color) = 0;
			virtual int   PieRGBA           (Surface* pSurface, int x, int y, int radius,  int start, int end, int r, int g, int b, int a) = 0;
			virtual int   FilledPieColor    (Surface* pSurface, int x, int y, int radius, int start, int end, const Color& color) = 0;
			virtual int   FilledPieRGBA     (Surface* pSurface, int x, int y, int radius, int start, int end, int r, int g, int b, int a) = 0;

			// Polygon
			virtual int   SimpleTriangle      (Surface* pSurface, int  x, int  y, int size, Orientation o, const Color& color) = 0;
			virtual int   TrigonColor         (Surface* pSurface, int x1, int y1, int x2, int y2, int x3, int y3, const Color& color) = 0;
			virtual int   TrigonRGBA          (Surface* pSurface, int x1, int y1, int x2, int y2, int x3, int y3, int r, int g, int b, int a) = 0;
			virtual int   AATrigonColor       (Surface* pSurface, int x1, int y1, int x2, int y2, int x3, int y3, const Color& color) = 0;
			virtual int   AATrigonRGBA        (Surface* pSurface, int x1, int y1, int x2, int y2, int x3, int y3, int r, int g, int b, int a) = 0;
			virtual int   FilledTrigonColor   (Surface* pSurface, int x1, int y1, int x2, int y2, int x3, int y3, const Color& color) = 0;
			virtual int   FilledTrigonRGBA    (Surface* pSurface, int x1, int y1, int x2, int y2, int x3, int y3, int r, int g, int b, int a) = 0;
			virtual int   PolygonColor        (Surface* pSurface, const int* vx, const int* vy, int n, const Color& color) = 0;
			virtual int   PolygonRGBA         (Surface* pSurface, const int* vx, const int* vy, int n, int r, int g, int b, int a) = 0;
			virtual int   AAPolygonColor      (Surface* pSurface, const int* vx, const int* vy, int n, const Color& color) = 0;
			virtual int   AAPolygonRGBA       (Surface* pSurface, const int* vx, const int* vy, int n, int r, int g, int b, int a) = 0;
			virtual int   FilledPolygonColor  (Surface* pSurface, const int* vx, const int* vy, int n, const Color& color) = 0;
			virtual int   FilledPolygonRGBA   (Surface* pSurface, const int* vx, const int* vy, int n, int r, int g, int b, int a) = 0;
			virtual int   TexturedPolygon     (Surface* pSurface, const int* vx, const int* vy, int n, Surface* pTexture,int texture_dx,int texture_dy) = 0;
			virtual int   FilledPolygonColorMT(Surface* pSurface, const int* vx, const int* vy, int n, const Color& color, int** polyInts, int* polyAllocated) = 0;
			virtual int   FilledPolygonRGBAMT (Surface* pSurface, const int* vx, const int* vy, int n, int r, int g, int b, int a, int** polyInts, int* polyAllocated) = 0;
			virtual int   TexturedPolygonMT   (Surface* pSurface, const int* vx, const int* vy, int n, Surface* pTexture, int texture_dx, int texture_dy, int** polyInts, int* polyAllocated) = 0;


			///////////////////////////////////////////////////////////////////////
			// Resampling
			///////////////////////////////////////////////////////////////////////

			// Zoom
			// Zooms a 32bit or 8bit 'src' surface to newly created 'dst' surface.
			// 'zoomx' and 'zoomy' are scaling factors for width and height. If 'smooth' is 1
			// then the destination 32bit surface is anti-aliased. If the surface is not 8bit
			// or 32bit RGBA/ABGR it will be converted into a 32bit RGBA format on the fly.
			// zoomX/zoomY can be less than 1.0 for shrinking.
			virtual Surface* ZoomSurface(Surface* pSurface, double zoomx, double zoomy, bool bSmooth) = 0;

			// Returns the size of the target surface for a zoomSurface() = 0 call
			virtual void ZoomSurfaceSize(int width, int height, double zoomx, double zoomy, int* dstwidth, int* dstheight) = 0;

			// Shrinks a 32bit or 8bit surface to a newly created surface.
			// 'factorX' and 'factorY' are the shrinking ratios (i.e. 2 = 1/2 the size,
			// 3 = 1/3 the size, etc.) The destination surface is antialiased by averaging
			// the source box RGBA or Y information. If the surface is not 8bit
			// or 32bit RGBA/ABGR it will be converted into a 32bit RGBA format on the fly.
			virtual Surface* ShrinkSurface(Surface* pSurface, int factorX, int factorY) = 0;

			// Returns a rotated surface by 90, 180, or 270 degrees.
			virtual Surface* RotateSurface90Degrees(Surface* pSurface, int nClockwiseTurns) = 0;

			// Creates a surface that is the same as pSource but with surfaceAlpha multiplied into pSource.
			virtual Surface* CreateTransparentSurface(Surface* pSource, int surfaceAlpha) = 0;


			///////////////////////////////////////////////////////////////////////
			// Blit functions
			///////////////////////////////////////////////////////////////////////

			// Generates rectSourceResult and rectDestResult from source and dest
			// surfaces and unclipped rectangles.
			virtual bool ClipForBlit(Surface* pSource, const Rect* pRectSource, Surface* pDest, const Rect* pRectDest, Rect& rectSourceResult, Rect& rectDestResult) = 0;

			// Does a 1:1 blit from pRectSource in pSource to pRectDest in pDest. 
			// Handles the case whereby pRectSource and pRectDest may refer to out of bounds of 
			// pSource and pDest, respectively.
			// If pDestClipRect is non-NULL, the output is further clipped to pDestClipRect.
			// pDestClipRect is not quite the same as pRectDest, as it's sometimes 
			// useful to blit a source rect to a dest rect but have it clip to another rect.
			// Returns 0 if OK or a negative error code.
			virtual int Blit(Surface* pSource, const Rect* pRectSource, Surface* pDest, const Rect* pRectDest, const Rect* pDestClipRect = NULL, const bool additiveBlend = false) = 0;

			// Does a 1:1 blit from pSource to pDest with the assumption that pRectSource and 
			// pRectDest are already clipped to pSource and pDest, respectively.
			// Returns 0 if OK or a negative error code.
			virtual int BlitNoClip(Surface* pSource, const Rect* pRectSource, Surface* pDest, const Rect* pRectDestz, const bool additiveBlend = false) = 0;

			//////////////////////////////////////////////////////////////////////////
			/// Blit a repeating pattern.
			/// The offsetX/Y position is the location within pRectDest that 
			/// the source origin will be. The blit is clipped to within pRectDest.
			/// If pRectSource is NULL then the entire Source is used, else the part
			/// of pSource that is tiled into pDest is the defined by pRectSource.
			virtual int BlitTiled(Surface* pSource, const Rect* pRectSource, Surface* pDest, const Rect* pRectDest, int offsetX, int offsetY) = 0;

			//////////////////////////////////////////////////////////////////////////
			/// This function stretches an image over a rectangular region
			/// by repeating (tiling) the center portion of the image.
			/// This is useful for drawing arbitrarily sized GUI buttons and
			/// scrollbar parts.
			///
			/// The image is divided like so:
			/// +-----------+-----------+-----------+
			/// | TL Corner |   Top     | TR Corner |
			/// |           |   Edge    |           |
			/// +-----------+-----------+-----------+
			/// | Left      |   Center  | Right     |
			/// | Edge      |           | Edge      |
			/// +-----------+-----------+-----------+
			/// | BL Corner |   Bottom  | BR Corner |
			/// |           |   Edge    |           |
			/// +-----------+-----------+-----------+
			/// The center portions will be tiled to cover the destination
			/// rectangle, where the edge portions will only be blitted
			/// once.
			///
			/// Parameters:
			///   pDest - the target drawing context.
			///   pRectDest - the destination rectangle to fill
			///   pImage - the source image to render.
			///   pRectSource - the source rectangle which has nine parts.
			///   pRectSourceCenter - Defines where the dividing lines between center
			///       and edges are (For example, mLeft controls the position
			///       of the dividing line between left edge and center.) = 0
			virtual int BlitEdgeTiled(Surface* pSource, const Rect* pRectSource, Surface* pDest, const Rect* pRectDest, const Rect* pRectSourceCenter) = 0;

			// Sets up the blit function needed to blit pSource to pDest.
			// Normally you don't need to call this function, as the Surface class and Blit 
			// functions will do it automatically.
			virtual bool SetupBlitFunction(Surface* pSource, Surface* pDest) = 0;



			///////////////////////////////////////////////////////////////////////
			// Utility functions
			///////////////////////////////////////////////////////////////////////

			// The result parameter may refer to one of the source parameters.
			virtual bool IntersectRect(const Rect& a, const Rect& b, Rect& result) = 0;

			// A PPM file is a simple bitmap format which many picture viewers can read.
			virtual bool WritePPMFile(const char* pPath, Surface* pSurface, bool bAlphaOnly) = 0;

			virtual RGBA32 makeRGB(int32_t r, int32_t g, int32_t b) = 0;
			virtual RGBA32 makeRGBA(int32_t r, int32_t g, int32_t b, int32_t a) = 0;

		};



		class EARasterConcrete: public IEARaster
		{
		public:
			virtual ~EARasterConcrete()
			{

			}
			virtual void IntRectToEARect(const WKAL::IntRect& in, EA::Raster::Rect& out);
			virtual void EARectToIntRect(const EA::Raster::Rect& in, WKAL::IntRect& out);
			// Surface management
			virtual Surface*    CreateSurface();
			virtual Surface*    CreateSurface(int width, int height, PixelFormatType pft);
			virtual Surface*    CreateSurface(Surface* pSurface);
			virtual Surface*    CreateSurface(void* pData, int width, int height, int stride, PixelFormatType pft, bool bCopyData, bool bTakeOwnership);
			virtual void        DestroySurface(Surface* pSurface);

			// Color conversion
			virtual void        ConvertColor(NativeColor c, PixelFormatType cFormat, Color& result);
			virtual void        ConvertColor(int r, int g, int b, int a, Color& result);
			virtual NativeColor ConvertColor(const Color& c, PixelFormatType resultFormat);
			virtual void        ConvertColor(const Color& c, int& r, int& g, int& b, int& a);
			virtual void        ConvertColor(NativeColor c, const PixelFormat& pf, int& r, int& g, int& b, int& a);
			virtual void        ConvertColor(NativeColor c, const PixelFormat& pf, int& r, int& g, int& b);

			// Pixel functions
			virtual void  GetPixel                (Surface* pSurface, int x, int y, Color& color);
			virtual int   SetPixelSolidColor      (Surface* pSurface, int x, int y, const Color& color);
			virtual int   SetPixelSolidColorNoClip(Surface* pSurface, int x, int y, const Color& color);
			virtual int   SetPixelColor           (Surface* pSurface, int x, int y, const Color& color);
			virtual int   SetPixelColorNoClip     (Surface* pSurface, int x, int y, const Color& color);
			virtual int   SetPixelRGBA            (Surface* pSurface, int x, int y, int r, int g, int b, int a);
			virtual int   SetPixelRGBANoClip      (Surface* pSurface, int x, int y, int r, int g, int b, int a);

			// Rectangle functions
			virtual int   FillRectSolidColor      (Surface* pSurface, const Rect* pRect, const Color& color);
			virtual int   FillRectColor           (Surface* pSurface, const Rect* pRect, const Color& color);
			virtual int   RectangleColor          (Surface* pSurface, int x1, int y1, int x2, int y2, const Color& color);
			virtual int   RectangleColor          (Surface* pSurface, const EA::Raster::Rect& rect, const Color& c);
			virtual int   RectangleRGBA           (Surface* pSurface, int x1, int y1, int x2, int y2, int r, int g, int b, int a);

			//virtual int   BoxColor                (Surface* pSurface, int x1, int y1, int x2, int y2, const Color& color);
			//virtual int   BoxRGBA                 (Surface* pSurface, int x1, int y1, int x2, int y2, int r, int g, int b, int a);

			// Line functions
			virtual int   HLineSolidColor(Surface* pSurface, int x1, int x2, int  y, const Color& color);
			virtual int   HLineSolidRGBA (Surface* pSurface, int x1, int x2, int  y, int r, int g, int b, int a);
			virtual int   HLineColor     (Surface* pSurface, int x1, int x2, int  y, const Color& color);
			virtual int   HLineRGBA      (Surface* pSurface, int x1, int x2, int  y, int r, int g, int b, int a);
			virtual int   VLineSolidColor(Surface* pSurface, int  x, int y1, int y2, const Color& color);
			virtual int   VLineSolidRGBA (Surface* pSurface, int  x, int y1, int y2, int r, int g, int b, int a);
			virtual int   VLineColor     (Surface* pSurface, int  x, int y1, int y2, const Color& color);
			virtual int   VLineRGBA      (Surface* pSurface, int  x, int y1, int y2, int r, int g, int b, int a);
			virtual int   LineColor      (Surface* pSurface, int x1, int y1, int x2, int y2, const Color& color);
			virtual int   LineRGBA       (Surface* pSurface, int x1, int y1, int x2, int y2, int r, int g, int b, int a);
			virtual int   AALineColor    (Surface* pSurface, int x1, int y1, int x2, int y2, const Color& color, bool bDrawEndpoint);
			virtual int   AALineColor    (Surface* pSurface, int x1, int y1, int x2, int y2, const Color& color);
			virtual int   AALineRGBA     (Surface* pSurface, int x1, int y1, int x2, int y2, int r, int g, int b, int a);

			// Circle / Ellipse
			virtual int   CircleColor       (Surface* pSurface, int x, int y, int radius, const Color& color);
			virtual int   CircleRGBA        (Surface* pSurface, int x, int y, int radius, int r, int g, int b, int a);
			virtual int   ArcColor          (Surface* pSurface, int x, int y, int r, int start, int end, const Color& color);
			virtual int   ArcRGBA           (Surface* pSurface, int x, int y, int radius, int start, int end, int r, int g, int b, int a);
			virtual int   AACircleColor     (Surface* pSurface, int x, int y, int r, const Color& color);
			virtual int   AACircleRGBA      (Surface* pSurface, int x, int y, int radius, int r, int g, int b, int a);
			virtual int   FilledCircleColor (Surface* pSurface, int x, int y, int r, const Color& color);
			virtual int   FilledCircleRGBA  (Surface* pSurface, int x, int y, int radius, int r, int g, int b, int a);
			virtual int   EllipseColor      (Surface* pSurface, int x, int y, int rx, int ry, const Color& color);
			virtual int   EllipseRGBA       (Surface* pSurface, int x, int y, int rx, int ry, int r, int g, int b, int a);
			virtual int   AAEllipseColor    (Surface* pSurface, int xc, int yc, int rx, int ry, const Color& color);
			virtual int   AAEllipseRGBA     (Surface* pSurface, int x, int y, int rx, int ry, int r, int g, int b, int a);
			virtual int   FilledEllipseColor(Surface* pSurface, int x, int y, int rx, int ry, const Color& color);
			virtual int   FilledEllipseRGBA (Surface* pSurface, int x, int y, int rx, int ry, int r, int g, int b, int a);
			virtual int   PieColor          (Surface* pSurface, int x, int y, int radius, int start, int end, const Color& color);
			virtual int   PieRGBA           (Surface* pSurface, int x, int y, int radius,  int start, int end, int r, int g, int b, int a);
			virtual int   FilledPieColor    (Surface* pSurface, int x, int y, int radius, int start, int end, const Color& color);
			virtual int   FilledPieRGBA     (Surface* pSurface, int x, int y, int radius, int start, int end, int r, int g, int b, int a);

			// Polygon
			virtual int   SimpleTriangle      (Surface* pSurface, int  x, int  y, int size, Orientation o, const Color& color);
			virtual int   TrigonColor         (Surface* pSurface, int x1, int y1, int x2, int y2, int x3, int y3, const Color& color);
			virtual int   TrigonRGBA          (Surface* pSurface, int x1, int y1, int x2, int y2, int x3, int y3, int r, int g, int b, int a);
			virtual int   AATrigonColor       (Surface* pSurface, int x1, int y1, int x2, int y2, int x3, int y3, const Color& color);
			virtual int   AATrigonRGBA        (Surface* pSurface, int x1, int y1, int x2, int y2, int x3, int y3, int r, int g, int b, int a);
			virtual int   FilledTrigonColor   (Surface* pSurface, int x1, int y1, int x2, int y2, int x3, int y3, const Color& color);
			virtual int   FilledTrigonRGBA    (Surface* pSurface, int x1, int y1, int x2, int y2, int x3, int y3, int r, int g, int b, int a);
			virtual int   PolygonColor        (Surface* pSurface, const int* vx, const int* vy, int n, const Color& color);
			virtual int   PolygonRGBA         (Surface* pSurface, const int* vx, const int* vy, int n, int r, int g, int b, int a);
			virtual int   AAPolygonColor      (Surface* pSurface, const int* vx, const int* vy, int n, const Color& color);
			virtual int   AAPolygonRGBA       (Surface* pSurface, const int* vx, const int* vy, int n, int r, int g, int b, int a);
			virtual int   FilledPolygonColor  (Surface* pSurface, const int* vx, const int* vy, int n, const Color& color);
			virtual int   FilledPolygonRGBA   (Surface* pSurface, const int* vx, const int* vy, int n, int r, int g, int b, int a);
			virtual int   TexturedPolygon     (Surface* pSurface, const int* vx, const int* vy, int n, Surface* pTexture,int texture_dx,int texture_dy);
			virtual int   FilledPolygonColorMT(Surface* pSurface, const int* vx, const int* vy, int n, const Color& color, int** polyInts, int* polyAllocated);
			virtual int   FilledPolygonRGBAMT (Surface* pSurface, const int* vx, const int* vy, int n, int r, int g, int b, int a, int** polyInts, int* polyAllocated);
			virtual int   TexturedPolygonMT   (Surface* pSurface, const int* vx, const int* vy, int n, Surface* pTexture, int texture_dx, int texture_dy, int** polyInts, int* polyAllocated);

			virtual Surface* ZoomSurface(Surface* pSurface, double zoomx, double zoomy, bool bSmooth);
			virtual void ZoomSurfaceSize(int width, int height, double zoomx, double zoomy, int* dstwidth, int* dstheight);
			virtual Surface* ShrinkSurface(Surface* pSurface, int factorX, int factorY);
			virtual Surface* RotateSurface90Degrees(Surface* pSurface, int nClockwiseTurns);
			virtual Surface* CreateTransparentSurface(Surface* pSource, int surfaceAlpha);
			virtual bool ClipForBlit(Surface* pSource, const Rect* pRectSource, Surface* pDest, const Rect* pRectDest, Rect& rectSourceResult, Rect& rectDestResult);
			virtual int Blit(Surface* pSource, const Rect* pRectSource, Surface* pDest, const Rect* pRectDest, const Rect* pDestClipRect = NULL, const bool additiveBlend = false);
			virtual int BlitNoClip(Surface* pSource, const Rect* pRectSource, Surface* pDest, const Rect* pRectDest, const bool additiveBlend = false);
			virtual int BlitTiled(Surface* pSource, const Rect* pRectSource, Surface* pDest, const Rect* pRectDest, int offsetX, int offsetY);
			virtual int BlitEdgeTiled(Surface* pSource, const Rect* pRectSource, Surface* pDest, const Rect* pRectDest, const Rect* pRectSourceCenter);
			virtual bool SetupBlitFunction(Surface* pSource, Surface* pDest);
			virtual bool IntersectRect(const Rect& a, const Rect& b, Rect& result);
			virtual bool WritePPMFile(const char* pPath, Surface* pSurface, bool bAlphaOnly);
			virtual RGBA32 makeRGB(int32_t r, int32_t g, int32_t b);
			virtual RGBA32 makeRGBA(int32_t r, int32_t g, int32_t b, int32_t a);

		};


    } // namespace Raster

} // namespace EA



#endif // Header include guard
