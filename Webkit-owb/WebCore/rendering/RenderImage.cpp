/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Allan Sandfeld Jensen (kde@carewolf.com)
 *           (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

/*
* This file was modified by Electronic Arts Inc Copyright � 2009
*/

#include "config.h"
#include "RenderImage.h"

#include "BitmapImage.h"
#include "Document.h"
#include "GraphicsContext.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLMapElement.h"
#include "HTMLNames.h"
#include "HitTestResult.h"
#include "Page.h"
#include "RenderView.h"
#include "SystemTime.h"

using namespace std;

namespace WebCore {

static const double cInterpolationCutoff = 800. * 800.;
static const double cLowQualityTimeThreshold = 0.050; // 50 ms
#include <wtf/FastAllocBase.h>

class RenderImageScaleData: public WTF::FastAllocBase {
public:
    RenderImageScaleData(RenderImage* image, const IntSize& size, double time, bool lowQualityScale)
        : m_size(size)
        , m_time(time)
        , m_lowQualityScale(lowQualityScale)
        , m_highQualityRepaintTimer(image, &RenderImage::highQualityRepaintTimerFired)
    {
    }

    ~RenderImageScaleData()
    {
        m_highQualityRepaintTimer.stop();
    }
    
    const IntSize& size() const { return m_size; }
    double time() const { return m_time; }
    bool useLowQualityScale() const { return m_lowQualityScale; }
    Timer<RenderImage>& hiqhQualityRepaintTimer() { return m_highQualityRepaintTimer; }

    void setSize(const IntSize& s) { m_size = s; }
    void setTime(double t) { m_time = t; }
    void setUseLowQualityScale(bool b)
    {
        m_highQualityRepaintTimer.stop();
        m_lowQualityScale = b;
        if (b)
            m_highQualityRepaintTimer.startOneShot(cLowQualityTimeThreshold);
    }
    
private:
    IntSize m_size;
    double m_time;
    bool m_lowQualityScale;
    Timer<RenderImage> m_highQualityRepaintTimer;
};

class RenderImageScaleObserver: public WTF::FastAllocBase
{
public:
    static bool shouldImagePaintAtLowQuality(RenderImage*, const IntSize&);

    static void imageDestroyed(RenderImage* image)
    {
        if (gImages) {
            RenderImageScaleData* data = gImages->take(image);
            delete data;
            if (gImages->size() == 0) {
                delete gImages;
                gImages = 0;
            }
        }
    }
    
    static void highQualityRepaintTimerFired(RenderImage* image)
    {
        RenderImageScaleObserver::imageDestroyed(image);
        image->repaint();
    }
    
    static HashMap<RenderImage*, RenderImageScaleData*>* gImages;
};

bool RenderImageScaleObserver::shouldImagePaintAtLowQuality(RenderImage* image, const IntSize& size)
{
    // If the image is not a bitmap image, then none of this is relevant and we just paint at high
    // quality.
    if (!image->image() || !image->image()->isBitmapImage())
        return false;

    // Make sure to use the unzoomed image size, since if a full page zoom is in effect, the image
    // is actually being scaled.
    IntSize imageSize(image->image()->width(), image->image()->height());

    // Look ourselves up in the hashtable.
    RenderImageScaleData* data = 0;
    if (gImages)
        data = gImages->get(image);

    if (imageSize == size) {
        // There is no scale in effect.  If we had a scale in effect before, we can just delete this data.
        if (data) {
            gImages->remove(image);
            delete data;
        }
        return false;
    }

    // There is no need to hash scaled images that always use low quality mode when the page demands it.  This is the iChat case.
    if (image->document()->page()->inLowQualityImageInterpolationMode()) {
        double totalPixels = static_cast<double>(image->image()->width()) * static_cast<double>(image->image()->height());
        if (totalPixels > cInterpolationCutoff)
            return true;
    }

    // If there is no data yet, we will paint the first scale at high quality and record the paint time in case a second scale happens
    // very soon.
    if (!data) {
        data = new RenderImageScaleData(image, size, currentTime(), false);
        if (!gImages)
            gImages = new HashMap<RenderImage*, RenderImageScaleData*>;
        gImages->set(image, data);
        return false;
    }

    // We are scaled, but we painted already at this size, so just keep using whatever mode we last painted with.
    if (data->size() == size)
        return data->useLowQualityScale();

    // We have data and our size just changed.  If this change happened quickly, go into low quality mode and then set a repaint
    // timer to paint in high quality mode.  Otherwise it is ok to just paint in high quality mode.
    double newTime = currentTime();
    data->setUseLowQualityScale(newTime - data->time() < cLowQualityTimeThreshold);
    data->setTime(newTime);
    data->setSize(size);
    return data->useLowQualityScale();
}

HashMap<RenderImage*, RenderImageScaleData*>* RenderImageScaleObserver::gImages = 0;

void RenderImage::highQualityRepaintTimerFired(Timer<RenderImage>* timer)
{
    RenderImageScaleObserver::highQualityRepaintTimerFired(this);
}

using namespace HTMLNames;

RenderImage::RenderImage(Node* node)
    : RenderReplaced(node, IntSize(0, 0))
    , m_cachedImage(0)
{
    updateAltText();
}

RenderImage::~RenderImage()
{
    if (m_cachedImage)
        m_cachedImage->removeClient(this);
    RenderImageScaleObserver::imageDestroyed(this);
}

void RenderImage::setCachedImage(CachedImage* newImage)
{
    if (m_cachedImage == newImage)
        return;
    if (m_cachedImage)
        m_cachedImage->removeClient(this);
    m_cachedImage = newImage;
    if (m_cachedImage) {
        m_cachedImage->addClient(this);
        if (m_cachedImage->errorOccurred())
            imageChanged(m_cachedImage);
    }
}

// If we'll be displaying either alt text or an image, add some padding.
static const unsigned short paddingWidth = 4;
static const unsigned short paddingHeight = 4;

// Alt text is restricted to this maximum size, in pixels.  These are
// signed integers because they are compared with other signed values.
static const int maxAltTextWidth = 1024;
static const int maxAltTextHeight = 256;

// Sets the image height and width to fit the alt text.  Returns true if the
// image size changed.
bool RenderImage::setImageSizeForAltText(CachedImage* newImage /* = 0 */)
{
    int imageWidth = 0;
    int imageHeight = 0;
  
    // If we'll be displaying either text or an image, add a little padding.
    if (!m_altText.isEmpty() || newImage) {
        imageWidth = paddingWidth;
        imageHeight = paddingHeight;
    }
  
    if (newImage) {
        // imageSize() returns 0 for the error image.  We need the true size of the
        // error image, so we have to get it by grabbing image() directly.
        imageWidth += static_cast<int>(newImage->image()->width() * style()->effectiveZoom());
        imageHeight += static_cast<int>(newImage->image()->height() * style()->effectiveZoom());
    }
  
    // we have an alt and the user meant it (its not a text we invented)
    if (!m_altText.isEmpty()) {
        const Font& font = style()->font();
        imageWidth = max(imageWidth, min(font.width(TextRun(m_altText.characters(), m_altText.length())), maxAltTextWidth));
        imageHeight = max(imageHeight, min(font.height(), maxAltTextHeight));
    }
  
    IntSize imageSize = IntSize(imageWidth, imageHeight);
    if (imageSize == intrinsicSize())
        return false;

    setIntrinsicSize(imageSize);
    return true;
}

void RenderImage::imageChanged(WrappedImagePtr newImage)
{
    if (documentBeingDestroyed())
        return;

    if (hasBoxDecorations())
        RenderReplaced::imageChanged(newImage);
    
    if (newImage != imagePtr() || !newImage)
        return;

    bool imageSizeChanged = false;

    // Set image dimensions, taking into account the size of the alt text.
    if (errorOccurred())
        imageSizeChanged = setImageSizeForAltText(m_cachedImage);
    
    bool shouldRepaint = true;

    // Image dimensions have been changed, see what needs to be done
    if (imageSize(style()->effectiveZoom()) != intrinsicSize() || imageSizeChanged) {
        if (!errorOccurred())
            setIntrinsicSize(imageSize(style()->effectiveZoom()));

        // In the case of generated image content using :before/:after, we might not be in the
        // render tree yet.  In that case, we don't need to worry about check for layout, since we'll get a
        // layout when we get added in to the render tree hierarchy later.
        if (containingBlock()) {
            // lets see if we need to relayout at all..
            int oldwidth = m_width;
            int oldheight = m_height;
            if (!prefWidthsDirty())
                setPrefWidthsDirty(true);
            calcWidth();
            calcHeight();

            if (imageSizeChanged || m_width != oldwidth || m_height != oldheight) {
                shouldRepaint = false;
                if (!selfNeedsLayout())
                    setNeedsLayout(true);
            }

            m_width = oldwidth;
            m_height = oldheight;
        }
    }

    if (shouldRepaint)
        // FIXME: We always just do a complete repaint, since we always pass in the full image
        // rect at the moment anyway.
        repaintRectangle(contentBox());
}

void RenderImage::resetAnimation()
{
    if (m_cachedImage) {
        image()->resetAnimation();
        if (!needsLayout())
            repaint();
    }
}

void RenderImage::paintReplaced(PaintInfo& paintInfo, int tx, int ty)
{
    int cWidth = contentWidth();
    int cHeight = contentHeight();
    int leftBorder = borderLeft();
    int topBorder = borderTop();
    int leftPad = paddingLeft();
    int topPad = paddingTop();

    if (document()->printing() && !view()->printImages())
        return;

    GraphicsContext* context = paintInfo.context;

    if (!hasImage() || errorOccurred()) {
        if (paintInfo.phase == PaintPhaseSelection)
            return;

        if (cWidth > 2 && cHeight > 2) {
            // Draw an outline rect where the image should be.
            context->setStrokeStyle(SolidStroke);
            context->setStrokeColor(Color::lightGray);
            context->setFillColor(Color::transparent);
            context->drawRect(IntRect(tx + leftBorder + leftPad, ty + topBorder + topPad, cWidth, cHeight));

            bool errorPictureDrawn = false;
            int imageX = 0;
            int imageY = 0;
            // When calculating the usable dimensions, exclude the pixels of
            // the ouline rect so the error image/alt text doesn't draw on it.
            int usableWidth = cWidth - 2;
            int usableHeight = cHeight - 2;

            if (errorOccurred() && !image()->isNull() && (usableWidth >= image()->width()) && (usableHeight >= image()->height())) {
                // Center the error image, accounting for border and padding.
                int centerX = (usableWidth - image()->width()) / 2;
                if (centerX < 0)
                    centerX = 0;
                int centerY = (usableHeight - image()->height()) / 2;
                if (centerY < 0)
                    centerY = 0;
                imageX = leftBorder + leftPad + centerX + 1;
                imageY = topBorder + topPad + centerY + 1;
                context->drawImage(image(), IntPoint(tx + imageX, ty + imageY));
                errorPictureDrawn = true;
            }

            if (!m_altText.isEmpty()) {
                String text = m_altText;
                text.replace('\\', backslashAsCurrencySymbol());
                context->setFont(style()->font());
                context->setFillColor(style()->color());
                int ax = tx + leftBorder + leftPad;
                int ay = ty + topBorder + topPad;
                const Font& font = style()->font();
                int ascent = font.ascent();

                // Only draw the alt text if it'll fit within the content box,
                // and only if it fits above the error image.
                TextRun textRun(text.characters(), text.length());
                int textWidth = font.width(textRun);
                if (errorPictureDrawn) {
                    if (usableWidth >= textWidth && font.height() <= imageY)
                        context->drawText(textRun, IntPoint(ax, ay + ascent));
                } else if (usableWidth >= textWidth && cHeight >= font.height())
                    context->drawText(textRun, IntPoint(ax, ay + ascent));
            }
        }
    } else if (hasImage() && cWidth > 0 && cHeight > 0) {
        Image* img = image(cWidth, cHeight);
        if (!img || img->isNull())
            return;

#if PLATFORM(MAC)
        if (style()->highlight() != nullAtom && !paintInfo.context->paintingDisabled())
            paintCustomHighlight(tx - m_x, ty - m_y, style()->highlight(), true);
#endif

        IntSize contentSize(cWidth, cHeight);
        bool useLowQualityScaling = RenderImageScaleObserver::shouldImagePaintAtLowQuality(this, contentSize);
        IntRect rect(IntPoint(tx + leftBorder + leftPad, ty + topBorder + topPad), contentSize);
        HTMLImageElement* imageElt = (element() && element()->hasTagName(imgTag)) ? static_cast<HTMLImageElement*>(element()) : 0;
        CompositeOperator compositeOperator = imageElt ? imageElt->compositeOperator() : CompositeSourceOver;
        context->drawImage(image(cWidth, cHeight), rect, compositeOperator, useLowQualityScaling);
    }
}

int RenderImage::minimumReplacedHeight() const
{
    return errorOccurred() ? intrinsicSize().height() : 0;
}

HTMLMapElement* RenderImage::imageMap()
{
    HTMLImageElement* i = element() && element()->hasTagName(imgTag) ? static_cast<HTMLImageElement*>(element()) : 0;
    return i ? i->document()->getImageMap(i->useMap()) : 0;
}

bool RenderImage::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, int _x, int _y, int _tx, int _ty, HitTestAction hitTestAction)
{
    bool inside = RenderReplaced::nodeAtPoint(request, result, _x, _y, _tx, _ty, hitTestAction);

    if (inside && element()) {
        int tx = _tx + m_x;
        int ty = _ty + m_y;
        
        HTMLMapElement* map = imageMap();
        if (map) {
            // we're a client side image map
            inside = map->mapMouseEvent(_x - tx, _y - ty, IntSize(contentWidth(), contentHeight()), result);
            result.setInnerNonSharedNode(element());
        }
    }

    return inside;
}

void RenderImage::updateAltText()
{
    if (!element())
        return;

    if (element()->hasTagName(inputTag))
        m_altText = static_cast<HTMLInputElement*>(element())->altText();
    else if (element()->hasTagName(imgTag))
        m_altText = static_cast<HTMLImageElement*>(element())->altText();
}

bool RenderImage::isWidthSpecified() const
{
    switch (style()->width().type()) {
        case Fixed:
        case Percent:
            return true;
        case Auto:
        case Relative: // FIXME: Shouldn't this case return true?
        case Static:
        case Intrinsic:
        case MinIntrinsic:
            return false;
    }
    ASSERT(false);
    return false;
}

bool RenderImage::isHeightSpecified() const
{
    switch (style()->height().type()) {
        case Fixed:
        case Percent:
            return true;
        case Auto:
        case Relative: // FIXME: Shouldn't this case return true?
        case Static:
        case Intrinsic:
        case MinIntrinsic:
            return false;
    }
    ASSERT(false);
    return false;
}

int RenderImage::calcReplacedWidth() const
{
    if (imageHasRelativeWidth())
        if (RenderObject* cb = isPositioned() ? container() : containingBlock())
            setImageContainerSize(IntSize(cb->availableWidth(), cb->availableHeight()));

    int width;
    if (isWidthSpecified())
        width = calcReplacedWidthUsing(style()->width());
    else if (usesImageContainerSize())
        width = imageSize(style()->effectiveZoom()).width();
    else if (imageHasRelativeWidth())
        width = 0; // If the image is relatively-sized, set the width to 0 until there is a set container size.
    else
        width = calcAspectRatioWidth();

    int minW = calcReplacedWidthUsing(style()->minWidth());
    int maxW = style()->maxWidth().isUndefined() ? width : calcReplacedWidthUsing(style()->maxWidth());

    return max(minW, min(width, maxW));
}

int RenderImage::calcReplacedHeight() const
{
    int height;
    if (isHeightSpecified())
        height = calcReplacedHeightUsing(style()->height());
    else if (usesImageContainerSize())
        height = imageSize(style()->effectiveZoom()).height();
    else if (imageHasRelativeHeight())
        height = 0; // If the image is relatively-sized, set the height to 0 until there is a set container size.
    else
        height = calcAspectRatioHeight();

    int minH = calcReplacedHeightUsing(style()->minHeight());
    int maxH = style()->maxHeight().isUndefined() ? height : calcReplacedHeightUsing(style()->maxHeight());

    return max(minH, min(height, maxH));
}

int RenderImage::calcAspectRatioWidth() const
{
    IntSize size = intrinsicSize();
    if (!size.height())
        return 0;
    if (!hasImage() || errorOccurred())
        return size.width(); // Don't bother scaling.
    return RenderReplaced::calcReplacedHeight() * size.width() / size.height();
}

int RenderImage::calcAspectRatioHeight() const
{
    IntSize size = intrinsicSize();
    if (!size.width())
        return 0;
    if (!hasImage() || errorOccurred())
        return size.height(); // Don't bother scaling.
    return RenderReplaced::calcReplacedWidth() * size.height() / size.width();
}

void RenderImage::calcPrefWidths()
{
    ASSERT(prefWidthsDirty());

    m_maxPrefWidth = calcReplacedWidth() + paddingLeft() + paddingRight() + borderLeft() + borderRight();

    if (style()->width().isPercent() || style()->height().isPercent() || 
        style()->maxWidth().isPercent() || style()->maxHeight().isPercent() ||
        style()->minWidth().isPercent() || style()->minHeight().isPercent())
        m_minPrefWidth = 0;
    else
        m_minPrefWidth = m_maxPrefWidth;

    setPrefWidthsDirty(false);
}

Image* RenderImage::nullImage()
{
    static BitmapImage sharedNullImage;
    return &sharedNullImage;
}

} // namespace WebCore
