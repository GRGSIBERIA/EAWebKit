/*
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

/*
* This file was modified by Electronic Arts Inc Copyright � 2009
*/

#include "config.h"
#include "HTMLCanvasElement.h"

#include "CanvasGradient.h"
#include "CanvasPattern.h"
#include "CanvasRenderingContext2D.h"
#include "CanvasStyle.h"
#include "Chrome.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "ImageBuffer.h"
#include "MIMETypeRegistry.h"
#include "Page.h"
#include "RenderHTMLCanvas.h"
#include "Settings.h"
#include <kjs/interpreter.h>
#include <math.h>
#include <stdio.h>
#include "EARaster.h"

#if PLATFORM(QT)
#include <QPainter>
#include <QPixmap>
#elif PLATFORM(CAIRO) && !PLATFORM(BAL)
#include <cairo.h>
#elif PLATFORM(BAL)
#include <BALBase.h>
#endif

namespace WebCore {

using namespace HTMLNames;

// These values come from the WhatWG spec.
static const int defaultWidth = 300;
static const int defaultHeight = 150;

// Firefox limits width/height to 32767 pixels, but slows down dramatically before it 
// reaches that limit. We limit by area instead, giving us larger maximum dimensions,
// in exchange for a smaller maximum canvas size.
const float HTMLCanvasElement::MaxCanvasArea = 32768 * 8192; // Maximum canvas area in CSS pixels

HTMLCanvasElement::HTMLCanvasElement(Document* doc)
    : HTMLElement(canvasTag, doc)
    , m_size(defaultWidth, defaultHeight)
    , m_observer(0)
    , m_originClean(true)
    , m_ignoreReset(false)
    , m_createdImageBuffer(false)
{
}

HTMLCanvasElement::~HTMLCanvasElement()
{
    if (m_2DContext)
        m_2DContext->detachCanvas();
}

#if ENABLE(DASHBOARD_SUPPORT)

HTMLTagStatus HTMLCanvasElement::endTagRequirement() const 
{
    Settings* settings = document()->settings();
    if (settings && settings->usesDashboardBackwardCompatibilityMode())
        return TagStatusForbidden; 

    return HTMLElement::endTagRequirement();
}

int HTMLCanvasElement::tagPriority() const 
{ 
    Settings* settings = document()->settings();
    if (settings && settings->usesDashboardBackwardCompatibilityMode())
        return 0; 

    return HTMLElement::tagPriority();
}

#endif

void HTMLCanvasElement::parseMappedAttribute(MappedAttribute* attr)
{
    const QualifiedName& attrName = attr->name();
    if (attrName == widthAttr || attrName == heightAttr)
        reset();
    HTMLElement::parseMappedAttribute(attr);
}

RenderObject* HTMLCanvasElement::createRenderer(RenderArena* arena, RenderStyle* style)
{
    Settings* settings = document()->settings();
    if (settings && settings->isJavaScriptEnabled()) {
        m_rendererIsCanvas = true;
        return new (arena) RenderHTMLCanvas(this);
    }

    m_rendererIsCanvas = false;
    return HTMLElement::createRenderer(arena, style);
}

void HTMLCanvasElement::setHeight(int value)
{
    setAttribute(heightAttr, String::number(value));
}

void HTMLCanvasElement::setWidth(int value)
{
    setAttribute(widthAttr, String::number(value));
}

String HTMLCanvasElement::toDataURL(const String& mimeType, ExceptionCode& ec)
{
    if (!m_originClean) {
        ec = SECURITY_ERR;
        return String();
    }

    if (m_size.isEmpty())
        return String("data:,");

    if (mimeType.isNull() || !MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType))
        return buffer()->toDataURL("image/png");

    return buffer()->toDataURL(mimeType);
}

CanvasRenderingContext* HTMLCanvasElement::getContext(const String& type)
{
    if (type == "2d") {
        if (!m_2DContext)
            m_2DContext = CanvasRenderingContext2D::create(this);
        return m_2DContext.get();
    }
    return 0;
}

void HTMLCanvasElement::willDraw(const FloatRect& rect)
{
    m_imageBuffer->clearImage();
    
    if (RenderObject* ro = renderer()) {
#ifdef CANVAS_INCREMENTAL_REPAINT
        // Handle CSS triggered scaling
        float widthScale = static_cast<float>(ro->width()) / static_cast<float>(m_size.width());
        float heightScale = static_cast<float>(ro->height()) / static_cast<float>(m_size.height());
        FloatRect r(rect.x() * widthScale, rect.y() * heightScale, rect.width() * widthScale, rect.height() * heightScale);
        ro->repaintRectangle(enclosingIntRect(r));
#else
        ro->repaint();
#endif
    }
    
    if (m_observer)
        m_observer->canvasChanged(this, rect);
}

void HTMLCanvasElement::reset()
{
    if (m_ignoreReset)
        return;

    bool ok;
    int w = getAttribute(widthAttr).toInt(&ok);
    if (!ok)
        w = defaultWidth;
    int h = getAttribute(heightAttr).toInt(&ok);
    if (!ok)
        h = defaultHeight;

    IntSize oldSize = m_size;
    m_size = IntSize(w, h);

    bool hadImageBuffer = m_createdImageBuffer;
    m_createdImageBuffer = false;
    m_imageBuffer.clear();
    if (m_2DContext)
        m_2DContext->reset();

    if (RenderObject* ro = renderer())
        if (m_rendererIsCanvas) {
            if (oldSize != m_size)
                static_cast<RenderHTMLCanvas*>(ro)->canvasSizeChanged();
            if (hadImageBuffer)
                ro->repaint();
        }
        
    if (m_observer)
        m_observer->canvasResized(this);
}

void HTMLCanvasElement::paint(GraphicsContext* context, const IntRect& r)
{
    if (context->paintingDisabled())
        return;

    if (m_imageBuffer) {
        Image* image = m_imageBuffer->image();
        if (image)
            context->drawImage(image, r);
    }
}

IntRect HTMLCanvasElement::convertLogicalToDevice(const FloatRect& logicalRect) const
{
    return IntRect(convertLogicalToDevice(logicalRect.location()), convertLogicalToDevice(logicalRect.size()));
}

IntSize HTMLCanvasElement::convertLogicalToDevice(const FloatSize& logicalSize) const
{
    float pageScaleFactor = document()->frame() ? document()->frame()->page()->chrome()->scaleFactor() : 1.0f;
    float wf = ceilf(logicalSize.width() * pageScaleFactor);
    float hf = ceilf(logicalSize.height() * pageScaleFactor);
    
    if (!(wf >= 1 && hf >= 1 && wf * hf <= MaxCanvasArea))
        return IntSize();

    return IntSize(static_cast<unsigned>(wf), static_cast<unsigned>(hf));
}

IntPoint HTMLCanvasElement::convertLogicalToDevice(const FloatPoint& logicalPos) const
{
    float pageScaleFactor = document()->frame() ? document()->frame()->page()->chrome()->scaleFactor() : 1.0f;
    float xf = logicalPos.x() * pageScaleFactor;
    float yf = logicalPos.y() * pageScaleFactor;
    
    return IntPoint(static_cast<unsigned>(xf), static_cast<unsigned>(yf));
}

void HTMLCanvasElement::createImageBuffer() const
{
    ASSERT(!m_imageBuffer);

    m_createdImageBuffer = true;
    
    FloatSize unscaledSize(width(), height());
    IntSize size = convertLogicalToDevice(unscaledSize);
    if (!size.width() || !size.height())
        return;
    m_imageBuffer.set(ImageBuffer::create(size, false).release());

    // 8/10/09 CSidhall Added buffer clear for some canvas draw operations are not supported and would draw a random memory
    ImageBuffer* pB = buffer(); 
    if(pB) {
        BalSurface* pS = pB->surface(); 
        if(pS){
            void *pData = pS->mpData;
            if(pData) {
#ifdef _DEBUG
                // Fill in with Red for debug to flag problems
                if(pS->mPixelFormat.mBytesPerPixel == 4){
                    int value = 0xffff0000; // Red
                    int *pDest = (int *) pData;
                    int size = pS->mWidth * pS->mHeight;
                    while(size--) {
                        *pDest = value;
                         pDest++;   
                    }
                }
#else
                // Fill with transparent in release
                memset(pData, 0x00, (pS->mStride * pS->mHeight)); 
#endif
        }
        }
    }
}

GraphicsContext* HTMLCanvasElement::drawingContext() const
{
    return buffer() ? m_imageBuffer->context() : 0;
}

ImageBuffer* HTMLCanvasElement::buffer() const
{
    if (!m_createdImageBuffer)
        createImageBuffer();
    return m_imageBuffer.get();
}

#if PLATFORM(CG)

CGImageRef HTMLCanvasElement::createPlatformImage() const
{
    GraphicsContext* context = drawingContext();
    if (!context)
        return 0;

    CGContextRef contextRef = context->platformContext();
    if (!contextRef)
        return 0;

    CGContextFlush(contextRef);

    return CGBitmapContextCreateImage(contextRef);
}

#elif PLATFORM(QT)

QPixmap HTMLCanvasElement::createPlatformImage() const
{
    if (!m_imageBuffer)
        return QPixmap();
    return *m_imageBuffer->pixmap();
}

#elif PLATFORM(CAIRO) && !PLATFORM(BAL)

cairo_surface_t* HTMLCanvasElement::createPlatformImage() const
{
    if (!m_imageBuffer)
        return 0;

    // Note that unlike CG, our returned image is not a copy or
    // copy-on-write, but the original. This is fine, since it is only
    // ever used as a source.

    cairo_surface_flush(m_imageBuffer->surface());
    cairo_surface_reference(m_imageBuffer->surface());
    return m_imageBuffer->surface();
}

#elif PLATFORM(BAL)

BalSurface * HTMLCanvasElement::createPlatformImage() const
{
    return 0;
}
#endif

}
