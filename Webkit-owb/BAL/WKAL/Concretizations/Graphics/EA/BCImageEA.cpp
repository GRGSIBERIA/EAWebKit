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
// BCImageEA.cpp
// By Paul Pedriana
///////////////////////////////////////////////////////////////////////////////


#include "config.h"
#include "BitmapImage.h"

#include "AffineTransform.h"
#include "FloatRect.h"
#include "GraphicsContext.h"
#include "ImageObserver.h"
#include <math.h>
#include "EARaster.h"
#include <EAWebKit/EAWebKit.h>
#include <EAWebKit/EAWebKitConfig.h>
#include "EARaster.h"
#include "EARasterColor.h"
#include "BCImageCompressionEA.h"

// This function loads resources from WebKit.
Vector<char> loadResourceIntoArray(const char*);


namespace WKAL {


// Get on/off blend status
// 10/28/09 CSidhall - Added support for image additive alpha blending.
static bool IsImageAdditiveBlendingActive(void)
{
    // We have the blend as a global param (could be also set on a view basis but we need a way to find the current active view)
    const EA::WebKit::Parameters& parameters = EA::WebKit::GetParameters();
    bool blendingFlag = parameters.mbEnableImageAdditiveAlphaBlending;
    return blendingFlag;
}


void FrameData::clear()
{
    if (m_frame)
    {
        EA::Raster::DestroySurface(m_frame);

        m_frame    = 0;
        m_duration = 0;
        m_hasAlpha = true;
    }
}


BitmapImage::BitmapImage(BalSurface* pSurface, ImageObserver* pObserver)
    : Image(pObserver)
    , m_currentFrame(0)
    , m_frames(0)
    , m_frameTimer(0)
    , m_repetitionCount(0)
    , m_repetitionsComplete(0)
    , m_isSolidColor(false)
    , m_animatingImageType(false)
    , m_animationFinished(true)
    , m_allDataReceived(true)
    , m_haveSize(true)
    , m_sizeAvailable(true)
    , m_decodedSize(0)
    , m_haveFrameCount(true)
    , m_frameCount(1)
{
    initPlatformData();

    const int width  = pSurface->mWidth;
    const int height = pSurface->mHeight;

    m_decodedSize = width * height * 4;
    m_size        = IntSize(width, height);

    m_frames.grow(1);
    m_frames[0].m_frame = pSurface;

    checkForSolidColor();
}


void BitmapImage::draw(GraphicsContext* context, const FloatRect& dst, const FloatRect& src, CompositeOperator op)
{
    // 11/09/09 CSidhall Added notify start of process to user
	NOTIFY_PROCESS_STATUS(EA::WebKit::kVProcessTypeDrawImage, EA::WebKit::kVProcessStatusStarted);
	
    // CSidhall 1/14/09 Removed pImage as const pointer so it can be replace by a decompressed version when needed
    EA::Raster::Surface* pImage = frameAtIndex(m_currentFrame);

    if (pImage) // If it's too early we won't have an image yet.
    {
        if (mayFillWithSolidColor())
            fillWithSolidColor(context, dst, solidColor(), op);
        else
        {
            const bool additive = IsImageAdditiveBlendingActive();
            
            // Set the compositing operation.
            if (op == CompositeSourceOver && !frameHasAlphaAtIndex(m_currentFrame))
                context->setCompositeOperation(CompositeCopy);
            else
                context->setCompositeOperation(op);

            EA::Raster::Surface* cr = context->platformContext();
            
            float scaleX = dst.width()  / src.width();
            float scaleY = dst.height() / src.height();

            // Draw the image.
            EA::Raster::Rect srcRect, dstRect;

            srcRect.x = (int)src.x();
            srcRect.y = (int)src.y();

            if (0 == src.width())
                srcRect.w = pImage->mWidth;
            else
                srcRect.w = (int)src.width();

            if (0 == src.height())
                srcRect.h = pImage->mHeight;
            else
                srcRect.h = (int)src.height();

            dstRect.x = (int)(dst.x() + context->origin().width());
            dstRect.y = (int)(dst.y() + context->origin().height());
            dstRect.w = (int)dst.width();
            dstRect.h = (int)dst.height();

            const bool bScaled = (scaleX != 1.0) || (scaleY != 1.0);

            if (bScaled)
            {
                srcRect.x = (int)(src.x() * scaleX);  // Note by Paul Pedriana: Is this right? It seems wrong to me. Why is it modifing the source rect x/y?
                srcRect.y = (int)(src.y() * scaleY);
                srcRect.w = dstRect.w;
                srcRect.h = dstRect.h;
            }

            // To do: Create scratch surfaces that are used for these operations. 

            // CSidhall 1/14//09 Added image decompression.  
            // The actual compression is in BitmapImage::cacheFrame() after an image has been fully loaded
            // This here just unpacks the full image into an allocated surface (which needs to be removed after the draw).
            #if EAWEBKIT_USE_RLE_COMPRESSION || EAWEBKIT_USE_YCOCGDXT5_COMPRESSION            
            
            EA::Raster::Surface* pDecompressedImage = BCImageCompressionEA::UnpackCompressedImage(pImage);         
            
            // Note: we are changing the image pointer here to the decompressed image instead!      
            if(pDecompressedImage != NULL)
                pImage = pDecompressedImage;
           
            #endif 

            // if (src != dst)  // Note by Paul Pedriana: Is this right? It seems wrong to me. Shouldn't it check for scaleX and scaleY?
            if (bScaled)
            {
                if (context->transparencyLayer() == 1.0)
                {
                    EA::Raster::Surface* const pZoomedSurface = EA::Raster::ZoomSurface(pImage, scaleX, scaleY, 0);

                    EA::Raster::Blit(pZoomedSurface, &srcRect, cr, &dstRect, NULL, additive);

                    EA::Raster::DestroySurface(pZoomedSurface);
                }
                else
                {
                    // Note by Paul Pedriana: Why is it applying transparency via a new image when instead it could 
                    // just do the blit with surface transparency added in? Is that not possible with SDL?
                    EA::Raster::Surface* const pAlphadSurface = EA::Raster::CreateTransparentSurface(pImage, static_cast<int>(context->transparencyLayer() * 255));
                    EA::Raster::Surface* const pZoomedSurface = EA::Raster::ZoomSurface(pAlphadSurface, scaleX, scaleY, 0);

                    EA::Raster::Blit(pZoomedSurface, &srcRect, cr, &dstRect, NULL, additive);

                    EA::Raster::DestroySurface(pZoomedSurface);
                    EA::Raster::DestroySurface(pAlphadSurface);
                }
            }
            else
            {
                if (context->transparencyLayer() == 1.0)
                    EA::Raster::Blit(pImage, &srcRect, cr, &dstRect, NULL, additive);
                else
                {
                    EA::Raster::Surface* const pAlphadSurface = CreateTransparentSurface(pImage, static_cast<int>(context->transparencyLayer() * 255));

                    EA::Raster::Blit(pAlphadSurface, &srcRect, cr, &dstRect, NULL, additive);

                    EA::Raster::DestroySurface(pAlphadSurface);
                }
            }
            
            // CSidhall 1/14//09 Added image decompression. This cleans up the allocated surface.
            #if EAWEBKIT_USE_RLE_COMPRESSION || EAWEBKIT_USE_YCOCGDXT5_COMPRESSION             
            
            // Remove the full ARBG buffer of decompressed data        
            if(pDecompressedImage != NULL)
                EA::Raster::DestroySurface(pDecompressedImage);
            
            #endif  

            startAnimation();

            if (imageObserver())
                imageObserver()->didDraw(this);
        }
    }

	NOTIFY_PROCESS_STATUS(EA::WebKit::kVProcessTypeDrawImage, EA::WebKit::kVProcessStatusEnded);
}


void Image::drawPattern(GraphicsContext* context, const FloatRect& tileRect, const AffineTransform& patternTransform,
                        const FloatPoint& phase, CompositeOperator op, const FloatRect& destRect)
{
    if (destRect.isEmpty())
        return;

    NOTIFY_PROCESS_STATUS(EA::WebKit::kVProcessTypeDrawImagePattern, EA::WebKit::kVProcessStatusStarted);
	
    // CSidhall - Removed pImage as const pointer so it can be changed for decompression
    EA::Raster::Surface* pImage = nativeImageForCurrentFrame();

    if (!pImage) // If it's too early we won't have an image yet.
        return;

    EA::Raster::Surface* const cr = context->platformContext();
    context->save();
    
    context->setCompositeOperation(op);

  
    // 2/9/09 CSidhall Height and width adjustments     
    // This is to fix a background clip bug when scrolling
    FloatRect adjDestRect = destRect;
    IntSize origin = context->origin();       
    float dy =  destRect.y() + origin.height();
    if(dy < 0.0f)
    {
        float heightAdj = adjDestRect.height() + dy;
        if(heightAdj <= 0.0f)
        {
            // Added to fix negative or 0 height clip bug 
            context->restore();
            return;
        }
        adjDestRect.setHeight(heightAdj);
    }
    float dx =  destRect.x() + origin.width();
    if(dx < 0.0f)
    {
        float widthAdj = adjDestRect.width() + dx;
        if(widthAdj <= 0.0f)
        {
            context->restore();
            return;
        }
        adjDestRect.setWidth(widthAdj);
    }

    // 2/11/09 CS Note: We want to grap the current clip rect before context->clip()
    EA::Raster::Rect clipRect= cr->mClipRect;

    context->clip(IntRect(adjDestRect)); // don't draw outside this

    IntRect dest(IntPoint(), IntSize(pImage->mWidth, pImage->mHeight));
    IntRect src(static_cast<int>(phase.x()), static_cast<int>(phase.y()), static_cast<int>(tileRect.size().width()), static_cast<int>(tileRect.size().height()));

    int xMax = static_cast<int>(destRect.x() + destRect.width());
    int yMax = static_cast<int>(destRect.y() + destRect.height());

    EA::Raster::Rect srcRect, dstRect;

    srcRect.x = 0;
    srcRect.y = 0;

    if (0 == src.width())
        srcRect.w = pImage->mWidth;
    else
        srcRect.w = src.width();

    if (0 == src.height())
        srcRect.h = pImage->mHeight;
    else
        srcRect.h = src.height();

    dstRect.x = dest.x();
    dstRect.y = dest.y();
    dstRect.w = dest.width();
    dstRect.h = dest.height();

    // compute ratio of the zoomed part:
    const double ratioW = (double)dest.width()  / (double)srcRect.w;
    const double ratioH = (double)dest.height() / (double)srcRect.h;

   // CSidhall 1/14//09 Added image decompression.
    #if EAWEBKIT_USE_RLE_COMPRESSION || EAWEBKIT_USE_YCOCGDXT5_COMPRESSION            
    
    EA::Raster::Surface* pDecompressedImage = BCImageCompressionEA::UnpackCompressedImage(pImage);         
    
    // Note: we are changing the image pointer here to the decompressed image!      
    if(pDecompressedImage != NULL)
        pImage = pDecompressedImage;
    
    #endif 

    EA::Raster::Surface* pSurface = NULL;

    if ((ratioW != 1.0) || (ratioH != 1.0))
    {
        pSurface = EA::Raster::ZoomSurface(pImage, ratioW, ratioH, 0);

        // Adjust offset to the new referentiel (zoomed)
        srcRect.x = (int)(src.x() * ratioW);
        srcRect.y = (int)(src.y() * ratioH);
    }

    const bool additive = IsImageAdditiveBlendingActive();

    // EA/Alex Mole, 12/22/09: Just create the transparent surface once, rather than once per tile
    // Create a transparency layer if we need one
    EA::Raster::Surface* pAlphadSurface = NULL;
    if( context->transparencyLayer() != 1.0 )
    {
        if( pSurface )
        {
            pAlphadSurface = EA::Raster::CreateTransparentSurface(pSurface, static_cast<int>(context->transparencyLayer() * 255));
        }
        else
        {
            pAlphadSurface = EA::Raster::CreateTransparentSurface(pImage, static_cast<int>(context->transparencyLayer() * 255));
        }
    }

    // Now decide on which surface we want to blit
    EA::Raster::Surface* pSurfaceToBlit = pAlphadSurface;
    if( !pSurfaceToBlit )
    {
        pSurfaceToBlit = pSurface;
    }
    if( !pSurfaceToBlit )
    {
        pSurfaceToBlit = pImage;
    }

    for (int x = static_cast<int>(phase.x()); x <= xMax; x += pImage->mWidth)
    {
        for (int y = static_cast<int>(phase.y()); y <= yMax; y += pImage->mHeight)
        {
            dest.setLocation(IntPoint(x, y) + context->origin());
            
            dstRect.x = dest.x();
            dstRect.y = dest.y();
            dstRect.w = dest.width();
            dstRect.h = dest.height();

            EA::Raster::Blit(pSurfaceToBlit, &srcRect, cr, &dstRect, &clipRect, additive);
        }
    }

    if( pAlphadSurface )
    {
        EA::Raster::DestroySurface( pAlphadSurface );
    }

    if(pSurface)
        EA::Raster::DestroySurface(pSurface);

       #if EAWEBKIT_USE_RLE_COMPRESSION || EAWEBKIT_USE_YCOCGDXT5_COMPRESSION             
        
        // Remove the full ARBG buffer of decompressed data        
        if(pDecompressedImage != NULL)
            EA::Raster::DestroySurface(pDecompressedImage);
        
        #endif

    context->restore();

    if (imageObserver())
        imageObserver()->didDraw(this);

	NOTIFY_PROCESS_STATUS(EA::WebKit::kVProcessTypeDrawImagePattern, EA::WebKit::kVProcessStatusEnded);
}


void BitmapImage::initPlatformData()
{
}


void BitmapImage::invalidatePlatformData()
{
}


Image* Image::loadPlatformResource(const char* name)
{
    Vector<char> arr = loadResourceIntoArray(name);  // Note by Paul Pedriana: The current version of loadResourceIntoArray is a no-op.
    BitmapImage* img = new BitmapImage;
    RefPtr<SharedBuffer> buffer = SharedBuffer::create(arr.data(), arr.size());
    img->setData(buffer, true);
    return img;
}

// Checks to see if the image is of a solid color. We optimize these images and just do a fill rect instead.
void BitmapImage::checkForSolidColor()
{
    if (frameCount() == 1) // We could check multiple frames, but generally all that matters is the special case of a 1x1 single frame.
    {
        // CS - Removed pSurface pointer as const so can change it for decompressed versions
        EA::Raster::Surface* pSurface = frameAtIndex(0);

       // CSidhall 1/15//09 Added image decompression support here in case image is compressed already
        #if EAWEBKIT_USE_RLE_COMPRESSION || EAWEBKIT_USE_YCOCGDXT5_COMPRESSION            
        
        EA::Raster::Surface* pDecompressedImage = BCImageCompressionEA::UnpackCompressedImage(pSurface);         
        
        // Note: we are changing the image pointer here to the decompressed image instead      
        if(pDecompressedImage != NULL)
            pSurface = pDecompressedImage;
        
        #endif 

        if(pSurface && (pSurface->mWidth == 1) && (pSurface->mHeight == 1))
        {
            int      bpp = pSurface->mPixelFormat.mBytesPerPixel;
            uint8_t* p   = (uint8_t*)pSurface->mpData; //  + (0 * pSurface->pitch) + (0 * bpp);
            uint32_t color;

            switch (bpp)
            {
                case 3:
                    // Note by Paul Pedriana: I'm not sure this is correct.
                    color = (p[0] << 16) | (p[1] <<  8) | (p[0] << 0);  // ARGB
                    break;

                default:
                case 4:
                    color = *(uint32_t*)p; // This assumes p is 32 bit aligned.
                    break;
            }

            int r, g, b, a;
            EA::Raster::ConvertColor(color, pSurface->mPixelFormat, r, g, b, a);
            m_solidColor.setRGB( EA::Raster::makeRGBA(r, g, b, a) );
        }
            
        // CSidhall 1/14/09 Added image decompression
        #if EAWEBKIT_USE_RLE_COMPRESSION || EAWEBKIT_USE_YCOCGDXT5_COMPRESSION             
        
        // Remove the full ARBG buffer of decompressed data        
        if(pDecompressedImage != NULL)
            EA::Raster::DestroySurface(pDecompressedImage);
        
        #endif
    }
}

// CSidhall 7/14/09 - Added image lock for prune from decoder (false is not locked)
 bool BitmapImage::imagePruneLockStatus() const
 {
     return m_source.imagePruneLockStatus();
 }


} // namespace

