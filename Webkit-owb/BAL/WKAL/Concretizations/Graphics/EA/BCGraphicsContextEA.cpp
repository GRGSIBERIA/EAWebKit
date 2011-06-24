/*
 * Copyright (C) 2008 Pleyo.  All rights reserved.
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
 * 3.  Neither the name of Pleyo nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PLEYO AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL PLEYO OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
* This file was modified by Electronic Arts Inc Copyright © 2008-2009
*/

///////////////////////////////////////////////////////////////////////////////
// BCGraphicsContextEA.cpp
// Modified by Paul Pedriana - 2008
///////////////////////////////////////////////////////////////////////////////


#include "config.h"
#include "GraphicsContext.h"

#include "AffineTransform.h"
#include "FloatRect.h"
#include "Font.h"
#include "ImageBuffer.h"
#include "IntRect.h"
#include "NotImplemented.h"
#include "Path.h"
#include "SimpleFontData.h"
#include <math.h>
#include <stdio.h>
#include <wtf/MathExtras.h>
#include <wtf/FastAllocBase.h>

#include "GraphicsContextPrivate.h"
#include "BCGraphicsContextPlatformPrivateEA.h"

#include <EARaster/EARaster.h>
#include <EARaster/EARasterColor.h>
#include <EAWebKit/EAWebKit.h>

#ifndef M_PI
    #define M_PI 3.14159265359
#endif


using std::min;
using std::max;


namespace WKAL {


GraphicsContext::GraphicsContext(PlatformGraphicsContext* cr)
    : m_common(createGraphicsContextPrivate())
    , m_data(new GraphicsContextPlatformPrivate)
{
    m_data->surface = cr;
    setPaintingDisabled(!cr);
}

GraphicsContext::~GraphicsContext()
{
     destroyGraphicsContextPrivate(m_common);
     delete m_data;
}

AffineTransform GraphicsContext::getCTM() const
{
    NotImplemented();
    AffineTransform a;
    return a;
}

PlatformGraphicsContext* GraphicsContext::platformContext() const
{
    return m_data->surface;
}


PlatformGraphicsContext* GraphicsContext::setPlatformContext(PlatformGraphicsContext* p)
{
    PlatformGraphicsContext* pPrev = m_data->surface;
    m_data->surface = p;
    return pPrev;
}

void GraphicsContext::savePlatformState()
{
    if (paintingDisabled())
        return;

    // 2/11/09 CSidhall - Save clip rect 
    m_common->state.setClipRectRestore(m_data->surface->mClipRect);    
  
    m_common->stack.append(m_common->state);
}

void GraphicsContext::restorePlatformState()
{
    if (paintingDisabled())
        return;

    if (m_common->stack.isEmpty()) {
        DS_WAR("ERROR void BCGraphicsContext::restore() stack is empty");
        return;
    }

    // 2/11/09 CSidhall - Restore the clip rect 
    m_data->surface->mClipRect = m_common->state.getClipRectRestore();

    m_common->state = m_common->stack.last();
    m_common->stack.removeLast();

    // CS - Removed this as it does not truly restore the previous clip
    // clip(clippingRect);

}


// Draws a filled rectangle with a stroked border.
void GraphicsContext::drawRect(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    EA::Raster::Surface* pSurface = m_data->surface;

    EA::Raster::Rect dstRect;
    dstRect.x = rect.x() + origin().width();
    dstRect.y = rect.y() + origin().height();
    dstRect.w = rect.width();
    dstRect.h = rect.height();

    if (fillColor().alpha())
    {
        if (!m_data->layers.isEmpty())
        {
            const EA::Raster::Color rectFillColor(fillColor().red(), fillColor().green(), fillColor().blue(), static_cast<int>(fillColor().alpha() * m_data->layers.last()));

            EA::Raster::FillRectColor(pSurface, &dstRect, rectFillColor);
        }
        else
        {
            const EA::Raster::Color rectFillColor(fillColor().rgb());
            EA::Raster::FillRectColor(pSurface, &dstRect, rectFillColor);
        }
    }

    if (strokeStyle() != NoStroke)
    {
        const WKAL::IntSize& o = origin();
        const int            x = o.width();
        const int            y = o.height();

        if (!m_data->layers.isEmpty())
        {
            EA::Raster::RectangleRGBA(pSurface,
                                      (rect.x() + x), 
                                      (rect.y() + y),
                                      (rect.x() + x  + rect.width() - 1), 
                                      (rect.y() + y + rect.height() - 1),
                                      strokeColor().red(), strokeColor().green(), strokeColor().blue(), static_cast<int>(strokeColor().alpha() * m_data->layers.last()));
        }
        else
        {
            EA::Raster::RectangleRGBA(pSurface,
                                      (rect.x() + x), 
                                      (rect.y() + y),
                                      (rect.x() + x  + rect.width() - 1), 
                                      (rect.y() + y + rect.height() - 1),
                                      strokeColor().red(), strokeColor().green(), strokeColor().blue(), strokeColor().alpha());
        }
    }
}


// This is only used to draw borders.
void GraphicsContext::drawLine(const IntPoint& point1, const IntPoint& point2)
{
    if (paintingDisabled())
        return;

    StrokeStyle style = strokeStyle();  // To do: Handle SolidStroke, DottedStroke, DashedStroke here.
    if (style == NoStroke)
        return;
    
    float width = strokeThickness();    // To do: Handle width here. // For odd widths, we may need to add in 0.5 to the appropriate x/y so that the float arithmetic works.
    if (width < 1)
        width = 1;

    IntPoint p1(point1 + origin());
    IntPoint p2(point2 + origin());
    Color    color = strokeColor();

    int alpha;

    if (!m_data->layers.isEmpty())
        alpha = static_cast<int>(strokeColor().alpha() * m_data->layers.last());
    else
        alpha = strokeColor().alpha();

    if (p1.y() == p2.y())
    {
        EA::Raster::LineRGBA(m_data->surface,
                             p1.x()    , p1.y(),
                             p2.x() - 1, p2.y(),
                             color.red(),
                             color.green(),
                             color.blue(),
                             alpha);
    }
    else
    {
        EA::Raster::LineRGBA(m_data->surface,
                             p1.x(), p1.y(),
                             p2.x(), p2.y(),
                             color.red(),
                             color.green(),
                             color.blue(),
                             alpha);
    }
}

// This method is only used to draw the little circles used in lists.
void GraphicsContext::drawEllipse(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    float yRadius = .5f * rect.height();
    float xRadius = .5f * rect.width();

    // Mean that we will draw a circle
    if (xRadius == yRadius) {
        strokeArc(rect, 0, 360);
        return;
    }

    if (strokeStyle() != NoStroke) {
        unsigned width = static_cast<unsigned int>(strokeThickness());
        if (width == 0)
            width++;
    }

    Color color = strokeColor();

    int alpha;

    if (!m_data->layers.isEmpty())
        alpha = static_cast<int>(strokeColor().alpha() * m_data->layers.last());
    else
        alpha = strokeColor().alpha();

    EA::Raster::EllipseRGBA(m_data->surface, 
                            (int)(rect.x() + origin().width() + xRadius),
                            (int)(rect.y() + origin().height() + yRadius),
                            (int)(xRadius),
                            (int)(yRadius),
                            color.red(),
                            color.green(),
                            color.blue(),
                            alpha);
}


// TODO: draw points instead of lines for nicer circles
inline void drawArc(EA::Raster::Surface* pSurface, const WebCore::Color color, int zone, int xc, int yc, float& x0, float& y0, float x1, float y1, bool doSwap = true)
{
    // Mean First draw => will not draw just a point.
    if (x0 != x1)
    {
        switch(zone)
        {
            case 0:
                EA::Raster::LineRGBA(pSurface,
                            static_cast<int>(xc + ceilf(x0)), static_cast<int>(yc - ceilf(y0)),
                            static_cast<int>(xc + ceilf(x1)), static_cast<int>(yc - ceilf(y1)),
                            color.red(),
                            color.green(),
                            color.blue(),
                            color.alpha());
                break;

            case 1:
                EA::Raster::LineRGBA(pSurface,
                            static_cast<int>(xc - ceilf(y0)), static_cast<int>(yc - ceilf(x0)),
                            static_cast<int>(xc - ceilf(y1)), static_cast<int>(yc - ceilf(x1)),
                            color.red(),
                            color.green(),
                            color.blue(),
                            color.alpha());
                break;

            case 2:
                EA::Raster::LineRGBA(pSurface,
                            static_cast<int>(xc - ceilf(x0)), static_cast<int>(yc + ceilf(y0)),
                            static_cast<int>(xc - ceilf(x1)), static_cast<int>(yc + ceilf(y1)),
                            color.red(),
                            color.green(),
                            color.blue(),
                            color.alpha());
                break;

            case 3:
                EA::Raster::LineRGBA(pSurface,
                            static_cast<int>(xc + ceilf(y0)), static_cast<int>(yc + ceilf(x0)),
                            static_cast<int>(xc + ceilf(y1)), static_cast<int>(yc + ceilf(x1)),
                            color.red(),
                            color.green(),
                            color.blue(),
                            color.alpha());
                break;
        }

        if (doSwap)
        {
            x0 = x1;
            y0 = y1;
        }
    }
}


void drawArc(EA::Raster::Surface* pSurface, const IntRect rect, uint16_t startAngle, uint16_t angleSpan, const WebCore::Color color)
{
    //
    //        |y          (This diagram is supposed to be a circle).
    //        |
    //       ...
    // z=1 .  |  . z=0      We know that there are 360 degrees in a circle. First we
    //   .    |    .        see that a circle is symetrical about the x axis, so
    //  .     |     .       only the first 180 degrees need to be calculated. Next
    //--.-----+-----.--     we see that it's also symetrical about the y axis, so now
    //  .     |     . x     we only need to calculate the first 90 degrees.
    //   .    |    .        Each 90° region is associated to a zone (z value). Zone where
    // z=2 .  |  . z=3      we will draw is defined from startAngle and angleSpan.
    //       ...            Thus after coord computation, we draw in the zone(s).
    //        |             Note: 0 <= alpha0, alpha1 <= 90
    //        |

    int   r       = (rect.width() - 1) / 2;
    int   xc      = rect.x() + r;
    int   yc      = rect.y() + (rect.height() - 1)/ 2;
    int   z0      = startAngle / 90;
    int   z1      = (startAngle + angleSpan) / 90;
    int   alpha0  = startAngle % 90;
    float xalpha0 = r * cosf((alpha0 * M_PI / 180));
    int   alpha1  = (startAngle + angleSpan) % 90;
    float xalpha1 = r * cosf((alpha1 * M_PI / 180));

    float x0, y0, x1, y1;

    if (z0 == z1)
    {
        // Draw in the same zone from angle = z0 * 90 + alpha0 to angle = z0 * 90 + alpha1
        x0 = xalpha0;
        y0 = sqrt(pow((float)r, 2.f) - pow(x0, 2.f));

        for (x1 = xalpha0; x1 >= xalpha1; x1--)
        {
            y1 = sqrt(pow((float)r, 2.f) - pow(x1, 2.f));
            drawArc(pSurface, color, z0, xc, yc, x0, y0, x1, y1);
        }
    }
    else if ((z1 - z0) == 1)
    {
        // Begin to draw in zone Z and end in zone Z + 1
        if (alpha1 < alpha0)
        {
            // Draw from angle = z1 * 90 to angle = z1 * 90 + alpha1
            // Then from angle = z0 * 90 + alpha0 to angle = z1 * 90
            x0 = r;
            y0 = 0;

            for (x1 = r; x1 >= xalpha1; x1--)
            {
                y1 = sqrt(pow(r, 2.f) - pow(x1, 2.f));
                drawArc(pSurface, color, z1, xc, yc, x0, y0, x1, y1);
            }

            x0 = xalpha0;
            y0 = sqrt(pow((float)r, 2.f) - pow(x0, 2.f));

            for (x1 = xalpha0; x1 >= 0; x1--)
            {
                y1 = sqrt(pow((float)r, 2.f) - pow(x1, 2.f));
                drawArc(pSurface, color, z0, xc, yc, x0, y0, x1, y1);
            }
        }
        else
        {
            // Compute a complete quarter of circle.
            // Draw in zone1 from 0 to alpha0
            // Draw in zone0 and zone1 from alpha0 to alpha1
            // Draw in zone0 from alpha1 to 90
            x0 = r;
            y0 = 0;

            for (x1 = r; x1 >= 0; x1--)
            {
                y1 = sqrt(pow((float)r, 2.f) - pow(x1, 2.f));

                if (x1 < xalpha1)
                    drawArc(pSurface, color, z0, xc, yc, x0, y0, x1, y1);
                else if (x1 > xalpha0)
                    drawArc(pSurface, color, z1, xc, yc, x0, y0, x1, y1);
                else {
                    drawArc(pSurface, color, z0, xc, yc, x0, y0, x1, y1, false);
                    drawArc(pSurface, color, z1, xc, yc, x0, y0, x1, y1);
                }
            }
        }
    }
    else
    {
        // Draw at least a complete quarter of circle and probably many more
        x0 = r;
        y0 = 0;

        for (x1 = r; x1 >= 0; x1--)
        {
            y1 = sqrt(pow((float)r, 2.f) - pow(x1, 2.f));

            if ((z1 - z0) >= 3)
                drawArc(pSurface, color, z1 - 2, xc, yc, x0, y0, x1, y1, false);

            if (x1 < xalpha1)
                drawArc(pSurface, color, z1 % 3, xc, yc, x0, y0, x1, y1, false);

            if (x1 < xalpha0)
                drawArc(pSurface, color, z0, xc, yc, x0, y0, x1, y1, false);

            drawArc(pSurface, color, z1 - 1, xc, yc, x0, y0, x1, y1);
        }
    }
}



// FIXME: This function needs to be adjusted to match the functionality on the Mac side.
// Note by Paul Pedriana: I don't know what this Mac thing means.
void GraphicsContext::strokeArc(const IntRect& rect, int startAngle, int angleSpan)
{
    if (paintingDisabled())
        return;

    if (strokeStyle() == NoStroke)
        return;

    //draw as many arcs as needed for the thickness
    //this is a quick hack until we re-implement the drawArc in lower level.
    float   thick    = strokeThickness() / 2.0;
    IntRect rectWork = rect;
    int     a0       = (startAngle) / 90;
  //int     a1       = (startAngle + angleSpan) / 90;

    int alpha;

    if (!m_data->layers.isEmpty())
        alpha = static_cast<int>(strokeColor().alpha() * m_data->layers.last());
    else
        alpha = strokeColor().alpha();

    WebCore::Color color(strokeColor().red(), strokeColor().green(), strokeColor().blue(), alpha);

    switch (a0)
    {
        case 0:
            rectWork.setX(rect.x() + origin().width());

            for (int i = 0; i < (int)thick; i++)
            {
                rectWork.setY(rect.y() + origin().height() + i);
                rectWork.setWidth(rect.width() - i);
                rectWork.setHeight(rect.height() - i);
                drawArc(m_data->surface, rectWork, startAngle, angleSpan, color);
            }
            break;

        case 1:
            for (int i = 0; i < (int)thick; i++)
            {
                rectWork.setX(rect.x() + origin().width() + i);
                rectWork.setY(rect.y() + origin().height() + i);
                rectWork.setWidth(rect.width() - i*2);
                rectWork.setHeight(rect.height() - i*2);
                drawArc(m_data->surface, rectWork, startAngle, angleSpan, color);
            }
            break;

        case 2:
            rectWork.setY(rect.y() + origin().height());

            for (int i = 0; i < (int)thick; i++)
            {
                rectWork.setX(rect.x() + origin().width() + i);
                rectWork.setWidth(rect.width() - i);
                rectWork.setHeight(rect.height() - i);
                drawArc(m_data->surface, rectWork, startAngle, angleSpan, color);
            }
            break;

        case 3:
            for (int i = 0; i < (int)thick; i++)
            {
                rectWork.setX(rect.x() + origin().width() + i);
                rectWork.setY(rect.y() + origin().height() + i);
                rectWork.setWidth(rect.width() - i*2);
                rectWork.setHeight(rect.height() - i*2);
                drawArc(m_data->surface, rectWork, startAngle, angleSpan, color);
            }
            break;
    }
}


void GraphicsContext::drawConvexPolygon(size_t npoints, const FloatPoint* points, bool shouldAntialias)
{
    if (paintingDisabled())
        return;

    if (npoints <= 1)
        return;

    // To do: Make this use local memory for the common cases. Use our own allocator otherwise.
    int* vx = WTF::fastNewArray<int> (npoints);
    int* vy = WTF::fastNewArray<int> (npoints);
    int  x1 = 0, x2 = 0, y1 = 0, y2 = 0;

    for(size_t i=0; i < npoints; i++)
    {
        vx[i] = static_cast<int16_t>(points[i].x() + origin().width());
        vy[i] = static_cast<int16_t>(points[i].y() + origin().height());
        x1 = min(x1, vx[i]);
        y1 = min(y1, vy[i]);
        x2 = max(x2, vx[i]);
        y2 = max(y2, vy[i]);
    }

    int alpha;

    if (!m_data->layers.isEmpty())
        alpha = static_cast<int>(strokeColor().alpha() * m_data->layers.last());
    else
        alpha = strokeColor().alpha();

    Color color = fillColor();

    EA::Raster::FilledPolygonRGBA(m_data->surface, vx, vy, npoints,
                                   color.red(),
                                   color.green(),
                                   color.blue(),
                                   alpha);
    WTF::fastDeleteArray<int> (vx);
    WTF::fastDeleteArray<int> (vy);

}


void GraphicsContext::fillRect(const IntRect& rectWK, const Color& color, bool solidFill)
{
    if (paintingDisabled())
        return;

    const IntSize&   o = origin();
    EA::Raster::Rect rect(rectWK.x() + o.width(), rectWK.y() + o.height(), rectWK.width(), rectWK.height());

    EA::Raster::Surface* const pSurface = m_data->surface;
    // 7/23/09 CSidhall - Added a solid fill option to force a full clear
    if(solidFill)
    {
        EA::Raster::FillRectSolidColor(pSurface, &rect, color);
    }
    else if (color.alpha())
    {
        // To do: What really want to do is modify the alpha of 'color' in place instead of create temporary.
        const int            alpha = m_data->layers.isEmpty() ? strokeColor().alpha() : static_cast<int>(strokeColor().alpha() * m_data->layers.last());
        const EA::Raster::Color c(color.red(), color.green(), color.blue(), alpha);

        EA::Raster::FillRectColor(pSurface, &rect, c);
    }
}


void GraphicsContext::fillRect(const FloatRect& rect, const Color& color)
{
    fillRect(IntRect(rect), color);
}


void GraphicsContext::clip(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    if (rect.isEmpty())
        m_data->surface->SetClipRect(NULL);
    else
    {
        float x = rect.x() + origin().width();
        x = (x > 0) ? x : 0;

        float y = rect.y() + origin().height();
        y = (y > 0) ? y : 0;

        const EA::Raster::Rect dstRect((int)x, (int)y, (int)rect.width(), (int)rect.height());
        m_data->surface->SetClipRect(&dstRect);
    }
}


// 3/13/09 CSidhall - Added to get easy access to the current clip rectangle in screen space
FloatRect GraphicsContext::getClip() const
{
     FloatRect rect;

    // Note: seems m_data might be decrepated eventually
    if((m_data) && (m_data->surface))
    {
        rect.setX( (float) m_data->surface->mClipRect.x);
        rect.setY((float) m_data->surface->mClipRect.y);
        rect.setWidth(m_data->surface->mClipRect.width());
        rect.setHeight(m_data->surface->mClipRect.height());
    }
    return rect;
}




// It's called a "focus ring" but it's not a ring, it's a (possibly rounded) rect around 
// something that is highlighted, like a text input.
void GraphicsContext::drawFocusRing(const Color& color)
{
    if (paintingDisabled())
        return;

    const int radius = (focusRingWidth() - 1) / 2;
    const int offset = radius + focusRingOffset();

    const Vector<IntRect>& rects     = focusRingRects();
    unsigned               rectCount = rects.size();
    IntRect                finalFocusRect;

    for (unsigned i = 0; i < rectCount; i++)
    {
        IntRect focusRect = rects[i];

        focusRect.setLocation(focusRect.location() + origin());
        focusRect.inflate(offset);
        finalFocusRect.unite(focusRect);
    }

    bool useDefaultFocusRingDraw = true;
    EA::WebKit::ViewNotification* pVN = EA::WebKit::GetViewNotification();

    EA::WebKit::FocusRingDrawInfo focusInfo;
    EA::Raster::IntRectToEARect(finalFocusRect, focusInfo.mFocusRect);
    focusInfo.mFocusRect.h -= 1;
    focusInfo.mFocusRect.w -= 1;
    focusInfo.mSuggestedColor.setRGB(color.rgb());
    focusInfo.mpSurface = m_data->surface;
    if(pVN != NULL)
    {
        useDefaultFocusRingDraw = !pVN->DrawFocusRing(focusInfo);
    }

    //default focus rect draw method
    if(useDefaultFocusRingDraw)
    {
        // Force the alpha to 50%. This matches what the Mac does with outline rings.
        const  EA::Raster::Color ringColor(color.red(), color.green(), color.blue(), 127);
        EA::Raster::RectangleColor(focusInfo.mpSurface, focusInfo.mFocusRect, ringColor);
    }
}


void GraphicsContext::drawLineForText(const IntPoint& startPoint, int width, bool printing)
{
    if (paintingDisabled())
        return;

    // 2/12//09 CSidhall - Removed orgin offset from startPoint for is already added in by drawLine()
    IntPoint point(startPoint);

    IntPoint endPoint = point + IntSize(width, 0);

    // NOTE we should adjust line to pixel boundaries
    drawLine(point, endPoint);
}

void GraphicsContext::drawLineForMisspellingOrBadGrammar(const IntPoint& origin, int width, bool grammar)
{
    NotImplemented();
}

FloatRect GraphicsContext::roundToDevicePixels(const FloatRect& frect)
{
    return frect;
}

void GraphicsContext::translate(float x, float y)
{
    if (paintingDisabled())
        return;
    m_common->state.origin += IntSize(static_cast<int>(x), static_cast<int>(y));
}

IntSize GraphicsContext::origin()
{
    return m_common->state.origin;
}

void GraphicsContext::setPlatformFillColor(const Color& col)
{
    // FIXME: this is probably a no-op but I'm not sure
    // notImplemented(); // commented-out because it's chatty and clutters output
    m_common->state.fillColor = col;
}

void GraphicsContext::setPlatformStrokeColor(const Color& col)
{
    // FIXME: this is probably a no-op but I'm not sure
    //notImplemented(); // commented-out because it's chatty and clutters output
    m_common->state.strokeColor = col;
}

void GraphicsContext::setPlatformStrokeThickness(float strokeThickness)
{
    m_common->state.strokeThickness = strokeThickness;
}

void GraphicsContext::setPlatformStrokeStyle(const StrokeStyle& strokeStyle)
{
    m_common->state.strokeStyle = strokeStyle;
}

void GraphicsContext::setURLForRect(const KURL& link, const IntRect& destRect)
{
    notImplemented();
}

void GraphicsContext::concatCTM(const AffineTransform& transform)
{
    NotImplemented();
}

void GraphicsContext::addInnerRoundedRectClip(const IntRect& rect, int thickness)
{
    NotImplemented();
}

void GraphicsContext::clipToImageBuffer(const FloatRect& rect, const ImageBuffer* imageBuffer)
{
    if (paintingDisabled())
        return;

    notImplemented();
}

void GraphicsContext::setPlatformShadow(IntSize const&, int, Color const&)
{
    notImplemented();
}

void GraphicsContext::clearPlatformShadow()
{
    notImplemented();
}

void GraphicsContext::beginTransparencyLayer(float opacity)
{
    if (paintingDisabled())
        return;
    m_data->layers.append(opacity);
    m_data->beginTransparencyLayer();
}

void GraphicsContext::endTransparencyLayer()
{
    if (paintingDisabled())
        return;

    m_data->layers.removeLast();
    m_data->endTransparencyLayer();
}

void GraphicsContext::clearRect(const FloatRect& rect, bool solidFill)
{
    if (paintingDisabled())
        return;

    IntRect rectangle(rect);

    // 7/22/09 CSidhall - the fillRect also adds the origin so we should not added it here...
    //    rectangle.setLocation(rectangle.location() + origin());
    fillRect(rectangle, Color::white, solidFill);
}

void GraphicsContext::strokeRect(const FloatRect& rect, float width)
{
    if (paintingDisabled())
        return;

    NotImplemented();
}

void GraphicsContext::setLineCap(LineCap lineCap)
{
    if (paintingDisabled())
        return;

    NotImplemented();
}

void GraphicsContext::setLineJoin(LineJoin lineJoin)
{
    if (paintingDisabled())
        return;

    NotImplemented();
}

void GraphicsContext::setMiterLimit(float miter)
{
    if (paintingDisabled())
        return;

    NotImplemented();
}

void GraphicsContext::setAlpha(float)
{
    notImplemented();
}

void GraphicsContext::setCompositeOperation(CompositeOperator op)
{
    if (paintingDisabled())
        return;

    NotImplemented();
}

void GraphicsContext::beginPath()
{
    if (paintingDisabled())
        return;

    NotImplemented();
}

void GraphicsContext::addPath(const Path& path)
{
    if (paintingDisabled())
        return;

    NotImplemented();
}

void GraphicsContext::clip(const Path& path)
{
    if (paintingDisabled())
        return;

    NotImplemented();
}

void GraphicsContext::clipOut(const Path& path)
{
    if (paintingDisabled())
        return;

    NotImplemented();
}

void GraphicsContext::rotate(float radians)
{
    if (paintingDisabled())
        return;

    NotImplemented();
}

void GraphicsContext::scale(const FloatSize& size)
{
    if (paintingDisabled())
        return;

    NotImplemented();
}

void GraphicsContext::clipOut(const IntRect& r)
{
    if (paintingDisabled())
        return;

    NotImplemented();
}

void GraphicsContext::clipOutEllipseInRect(const IntRect& r)
{
    if (paintingDisabled())
        return;

    NotImplemented();
}

void GraphicsContext::fillRoundedRect(const IntRect& r, const IntSize& topLeft, const IntSize& topRight, const IntSize& bottomLeft, const IntSize& bottomRight, const Color& color)
{
    if (paintingDisabled())
        return;

    NotImplemented();
}

void GraphicsContext::setBalExposeEvent(BalEventExpose* expose)
{
    m_expose = expose;
}

BalEventExpose* GraphicsContext::balExposeEvent() const
{
    return m_expose;
}

BalDrawable* GraphicsContext::balDrawable() const
{
    //if (!m_data->expose)
    //    return 0;

    return 0;//GDK_DRAWABLE(m_data->expose->window);
}

float GraphicsContext::transparencyLayer()
{
    if (!m_data->layers.isEmpty())
        return m_data->layers.last();
    else
        return 1.0;
}

IntPoint GraphicsContext::translatePoint(const IntPoint& point) const
{
    NotImplemented();
    return point;
}

void GraphicsContext::setUseAntialiasing(bool enable)
{
    if (paintingDisabled())
        return;

    // When true, use the default Cairo backend antialias mode (usually this
    // enables standard 'grayscale' antialiasing); false to explicitly disable
    // antialiasing. This is the same strategy as used in drawConvexPolygon().
    //cairo_set_antialias(m_data->cr, enable ? CAIRO_ANTIALIAS_DEFAULT : CAIRO_ANTIALIAS_NONE);
}

} // namespace WebCore

