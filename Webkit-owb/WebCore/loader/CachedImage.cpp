/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

/*
* This file was modified by Electronic Arts Inc Copyright � 2009
*/

#include "config.h"
#include "CachedImage.h"
#include "Cache.h"
#include "CachedResourceClient.h"
#include "CachedResourceClientWalker.h"
#include "DocLoader.h"
#include "Frame.h"
#include "Request.h"
#include "SystemTime.h"
#include <wtf/Vector.h>

#if PLATFORM(CG)
#include "PDFDocumentImage.h"
#endif

#if ENABLE(SVG_AS_IMAGE)
#include "SVGImage.h"
#endif

using std::max;

namespace WebCore {

CachedImage::CachedImage(const String& url)
    : CachedResource(url, ImageResource)
{
    m_image = 0;
    m_status = Unknown;
}

CachedImage::CachedImage(Image* image)
    : CachedResource(String(), ImageResource)
{
    m_image = image;
    m_status = Cached;
    m_loading = false;
}

CachedImage::~CachedImage()
{
    delete m_image;
}
    
void CachedImage::load(DocLoader* docLoader)
{
    if (!docLoader || docLoader->autoLoadImages())
        CachedResource::load(docLoader, true, false, true);
    else
        m_loading = false;
}

void CachedImage::addClient(CachedResourceClient* c)
{
    CachedResource::addClient(c);

    if (m_image && !m_image->rect().isEmpty())
        c->imageChanged(this);

    if (!m_loading)
        c->notifyFinished(this);
}

void CachedImage::allReferencesRemoved()
{
    if (m_image && !m_errorOccurred)
        m_image->resetAnimation();
}

Image* CachedImage::s_brokenImage = NULL;  // 4/16/09 CSidhall - Extracted from function for memory leak fix

static Image* brokenImage()
{
    // CS - old code location 
    //static Image* brokenImage;
    if (!CachedImage::s_brokenImage)
        CachedImage::s_brokenImage = Image::loadPlatformResource("missingImage");
    return CachedImage::s_brokenImage;
}

BitmapImage* CachedImage::s_pNullImage = NULL;  // 3/26/09 CSidhall - extracted from function for memory leak fix

static Image* nullImage()
{
   // CS- old code:
    //static BitmapImage nullImage;
    //return &nullImage;

    if(!CachedImage::s_pNullImage)
        CachedImage::s_pNullImage = new(BitmapImage);  
    return CachedImage::s_pNullImage;
}

void CachedImage::staticFinalize()
{
    if(s_pNullImage){
        delete s_pNullImage;  
        s_pNullImage = NULL;
    }
    if(s_brokenImage) {
        delete s_brokenImage;
        s_brokenImage = NULL;
    }
}

Image* CachedImage::image() const
{
    if (m_errorOccurred)
        return brokenImage();

    if (m_image)
        return m_image;

    return nullImage();
}

void CachedImage::setImageContainerSize(const IntSize& containerSize)
{
    if (m_image)
        m_image->setContainerSize(containerSize);
}

bool CachedImage::usesImageContainerSize() const
{
    if (m_image)
        return m_image->usesContainerSize();

    return false;
}

bool CachedImage::imageHasRelativeWidth() const
{
    if (m_image)
        return m_image->hasRelativeWidth();

    return false;
}

bool CachedImage::imageHasRelativeHeight() const
{
    if (m_image)
        return m_image->hasRelativeHeight();

    return false;
}

IntSize CachedImage::imageSize(float multiplier) const
{
    if (!m_image)
        return IntSize();
    if (multiplier == 1.0f)
        return m_image->size();
        
    // Don't let images that have a width/height >= 1 shrink below 1 when zoomed.
    bool hasWidth = m_image->size().width() > 0;
    bool hasHeight = m_image->size().height() > 0;
    int width = static_cast<int>(m_image->size().width() * (m_image->hasRelativeWidth() ? 1.0f : multiplier));
    int height = static_cast<int>(m_image->size().height() * (m_image->hasRelativeHeight() ? 1.0f : multiplier));
    if (hasWidth)
        width = max(1, width);
    if (hasHeight)
        height = max(1, height);
    return IntSize(width, height);
}

IntRect CachedImage::imageRect(float multiplier) const
{
    if (!m_image)
        return IntRect();
    if (multiplier == 1.0f)
        return m_image->rect();
        
    // Don't let images that have a width/height >= 1 shrink below 1 when zoomed.
    bool hasWidth = m_image->rect().width() > 0;
    bool hasHeight = m_image->rect().height() > 0;
    int width = static_cast<int>(m_image->rect().width() * (m_image->hasRelativeWidth() ? 1.0f : multiplier));
    int height = static_cast<int>(m_image->rect().height() * (m_image->hasRelativeHeight() ? 1.0f : multiplier));
    if (hasWidth)
        width = max(1, width);
    if (hasHeight)
        height = max(1, height);
    return IntRect(static_cast<int>(m_image->rect().x() * (m_image->hasRelativeWidth() ? 1.0f : multiplier)), static_cast<int>(m_image->rect().y() * (m_image->hasRelativeHeight() ? 1.0f : multiplier)), width, height);
}

void CachedImage::notifyObservers()
{
    CachedResourceClientWalker w(m_clients);
    while (CachedResourceClient *c = w.next())
        c->imageChanged(this);
}

void CachedImage::clear()
{
    destroyDecodedData();
    delete m_image;
    m_image = 0;
    setEncodedSize(0);
}

inline void CachedImage::createImage()
{
    // Create the image if it doesn't yet exist.
    if (m_image)
        return;
#if PLATFORM(CG)
    if (m_response.mimeType() == "application/pdf") {
        m_image = new PDFDocumentImage;
        return;
    }
#endif
#if ENABLE(SVG_AS_IMAGE)
    if (m_response.mimeType() == "image/svg+xml") {
        m_image = new SVGImage(this);
        return;
    }
#endif
    m_image = new BitmapImage(this);
}

void CachedImage::data(PassRefPtr<SharedBuffer> data, bool allDataReceived)
{
    m_data = data;

    createImage();

    bool sizeAvailable = false;

    // Have the image update its data from its internal buffer.
    // It will not do anything now, but will delay decoding until 
    // queried for info (like size or specific image frames).
    sizeAvailable = m_image->setData(m_data, allDataReceived);

    // Go ahead and tell our observers to try to draw if we have either
    // received all the data or the size is known.  Each chunk from the
    // network causes observers to repaint, which will force that chunk
    // to decode.
    if (sizeAvailable || allDataReceived) {
        if (m_image->isNull()) {
            // FIXME: I'm not convinced this case can even be hit.
            error();
            if (inCache())
                cache()->remove(this);
            return;
        }
        
        notifyObservers();

        if (m_image)
            setEncodedSize(m_image->data() ? m_image->data()->size() : 0);
    }
    
    if (allDataReceived) {
        m_loading = false;
        checkNotify();
    }
}

void CachedImage::error()
{
    clear();
    m_errorOccurred = true;
    notifyObservers();
    m_loading = false;
    checkNotify();
}

void CachedImage::checkNotify()
{
    if (m_loading)
        return;

    CachedResourceClientWalker w(m_clients);
    while (CachedResourceClient* c = w.next())
        c->notifyFinished(this);
}

void CachedImage::destroyDecodedData()
{
    if (m_image && !m_errorOccurred)
        m_image->destroyDecodedData();
}

void CachedImage::decodedSizeChanged(const Image* image, int delta)
{
    if (image != m_image)
        return;
    
    setDecodedSize(decodedSize() + delta);
}

void CachedImage::didDraw(const Image* image)
{
    if (image != m_image)
        return;
    
    double timeStamp = Frame::currentPaintTimeStamp();
    if (!timeStamp) // If didDraw is called outside of a Frame paint.
        timeStamp = currentTime();
    
    CachedResource::didAccessDecodedData(timeStamp);
}

bool CachedImage::shouldPauseAnimation(const Image* image)
{
    if (image != m_image)
        return false;
    
    CachedResourceClientWalker w(m_clients);
    while (CachedResourceClient* c = w.next()) {
        if (c->willRenderImage(this))
            return false;
    }

    return true;
}

void CachedImage::animationAdvanced(const Image* image)
{
    if (image == m_image)
        notifyObservers();
}

} //namespace WebCore
