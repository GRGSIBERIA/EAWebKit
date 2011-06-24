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
// BCPlatformScrollBarEA.cpp
// By Paul Pedriana - 2008
///////////////////////////////////////////////////////////////////////////////


#include "config.h"
#include "PlatformScrollBar.h"

#include "IntRect.h"
#include "GraphicsContext.h"
#include "Frame.h"
#include "FrameView.h"
#include "PlatformMouseEvent.h"
#include "EventHandler.h"
#include "EARaster.h"
#include <EAWebKit/EAWebKit.h>


using namespace WebCore;
static int cHorizontalWidth = 15;
static int cHorizontalHeight = 15;
static int cVerticalWidth = 15;
static int cVerticalHeight = 15;
static int cHorizontalButtonWidth = 15;
static int cVerticalButtonHeight = 15;
static int cRealButtonLength = 28;
static int cButtonInset = 14;
static int cButtonHitInset = 3;
//static int cButtonLength = 10;
static int cThumbWidth = 15;
static int cThumbHeight = 15;
static int cThumbMinLength = 26;
static int cArrowInset = 5;


const double cInitialTimerDelay = 0.25;
const double cNormalTimerDelay = 0.05;

static int vSize = 0;
static int hSize = 0;

using std::max;
using std::min;

PlatformScrollbar::PlatformScrollbar(ScrollbarClient* client, ScrollbarOrientation orientation,
                                     ScrollbarControlSize controlSize)
    : Scrollbar(client, orientation, controlSize)
    , m_hoveredPart(NoPart)
    , m_pressedPart(NoPart)
    , m_pressedPos(0)
    , m_scrollTimer(this, &PlatformScrollbar::autoscrollTimerFired)
{
    vSize = Widget::height();
    hSize = Widget::width();
    if (orientation == VerticalScrollbar)
        setFrameGeometry(IntRect(0, 0, cVerticalWidth, cVerticalHeight));
    else
        setFrameGeometry(IntRect(0, 0, cHorizontalWidth, cHorizontalHeight));
}

PlatformScrollbar::~PlatformScrollbar()
{
    stopTimerIfNeeded();
}

int PlatformScrollbar::width() const
{
    return Widget::width();
}

int PlatformScrollbar::height() const
{
    return Widget::height();
}

void PlatformScrollbar::setEnabled(bool enabled)
{
    Widget::setEnabled(enabled);
}

static IntRect trackRepaintRect(const IntRect& trackRect, ScrollbarOrientation orientation, ScrollbarControlSize controlSize)
{
    const int cButtonLength = (orientation == VerticalScrollbar) ? cVerticalButtonHeight : cHorizontalButtonWidth;

    IntRect paintRect(trackRect);
    if (orientation == HorizontalScrollbar)
        paintRect.inflateX(cButtonLength);
    else
        paintRect.inflateY(cButtonLength);

    return paintRect;
}

static IntRect buttonRepaintRect(const IntRect& buttonRect, ScrollbarOrientation orientation, ScrollbarControlSize controlSize, bool start)
{
    IntRect paintRect(buttonRect);
    if (orientation == HorizontalScrollbar) {
        paintRect.setWidth(cRealButtonLength);
        if (!start)
            paintRect.setX(buttonRect.x() - (cRealButtonLength - buttonRect.width()));
    } else {
        paintRect.setHeight(cRealButtonLength);
        if (!start)
            paintRect.setY(buttonRect.y() - (cRealButtonLength - buttonRect.height()));
    }

    return paintRect;
}

bool PlatformScrollbar::hasButtons() const
{
    return isEnabled() && (m_orientation == HorizontalScrollbar ? width() : height()) >= 2 * (cRealButtonLength - cButtonHitInset);
}


IntRect PlatformScrollbar::trackRect() const
{
    if (m_orientation == HorizontalScrollbar) {
        if (!hasButtons())
            return IntRect(x(), y(), width(), cHorizontalHeight);
        return IntRect(x() + cHorizontalButtonWidth, y(), width() - 2 * cHorizontalButtonWidth, cHorizontalHeight);
    }

    if (!hasButtons())
        return IntRect(x(), y(), cVerticalWidth, height());
    return IntRect(x(), y() + cVerticalButtonHeight, cVerticalWidth, height() - 2 * cVerticalButtonHeight);
}

void PlatformScrollbar::paintTrack(GraphicsContext* context, const IntRect& rect, bool start, const IntRect& damageRect) const
{
    IntRect paintRect = hasButtons() ? trackRepaintRect(rect, m_orientation, controlSize()) : rect;
    
    if (!damageRect.intersects(paintRect))
        return;

    context->save();
    context->drawRect(rect);
    context->fillRect(rect, Color::darkGray);
    context->restore();
}

void PlatformScrollbar::paintRectWithBorder(GraphicsContext* context, const IntRect& rect, const Color& innerColor, const Color& outerColor) const
{
    context->drawRect(rect);
    context->fillRect(rect, Color::gray);

    Color tmp = context->strokeColor();

    context->setStrokeColor(Color::lightGray);
    context->drawLine( rect.topLeft(), rect.topRight() );
    context->drawLine( rect.topLeft(), rect.bottomLeft() );
    context->drawLine( rect.topRight(), rect.bottomRight() );
    context->drawLine( rect.bottomLeft(), rect.bottomRight() );

    IntRect inner( rect.x()+1, rect.y()+1, rect.width()-2, rect.height()-2 );
    context->setStrokeColor(Color::darkGray);
    context->drawLine( inner.topLeft(), inner.topRight() );
    context->drawLine( inner.topLeft(), inner.bottomLeft() );
    context->drawLine( inner.topRight(), inner.bottomRight() );
    context->drawLine( inner.bottomLeft(), inner.bottomRight() );

    context->setStrokeColor(tmp);
}

void PlatformScrollbar::paintArrowPoint(GraphicsContext* context, IntPoint& peak, IntPoint& pA, IntPoint& pB, const IntPoint& drift) const
{
    int i;
    for(i=0;i<2;++i)
    {
        context->drawLine(peak, pA);
        context->drawLine(peak, pB);
        peak.move(drift.x(), drift.y());
        pA.move(drift.x(), drift.y());
        pB.move(drift.x(), drift.y());
    }
}

void PlatformScrollbar::paintButton(GraphicsContext* context, const IntRect& rect, bool start, const IntRect& damageRect) const
{
    IntRect paintRect = buttonRepaintRect(rect, m_orientation, controlSize(), start);
    
    if (!damageRect.intersects(paintRect))
        return;

    context->save();

    // Draw the square with a border.
    context->setStrokeStyle(SolidStroke);
    paintRectWithBorder(context, rect, Color::gray, Color::lightGray);

    // Draw the arrow in one of four directions.
    EA::Raster::Orientation o;
    IntRect     inner( rect.x()+cArrowInset, rect.y()+cArrowInset, rect.width()-(2*cArrowInset), rect.height()-(2*cArrowInset) );
    int         arrowX;
    int         arrowY;

    if (start) // If the button is the up or left button as opposed to the down or right button...
    {
        if (m_orientation == HorizontalScrollbar) // If we are drawing a [<] arrow...
        {
            o = EA::Raster::kOLeft;
            arrowX = inner.x() + 1;
            arrowY = (inner.bottom() + inner.y()) / 2;
        } 
        else // else we are drawing a [^] arrow...
        {
            o = EA::Raster::kOUp;
            arrowX = (inner.x() + inner.right()) / 2;
            arrowY = inner.y() + 1;
        }
    } 
    else 
    {
        if (m_orientation == HorizontalScrollbar) // If we are drawing a [>] arrow...
        {
            o = EA::Raster::kORight;
            arrowX = inner.right() - 1;
            arrowY = (inner.bottom() + inner.y()) / 2;
        } 
        else // Else we are drawing a [v] arrow...
        {
            o = EA::Raster::kODown;
            arrowX = (inner.x() + inner.right()) / 2;
            arrowY = inner.bottom() - 1;
        }
    }

    EA::Raster::Surface* pSurface = context->platformContext();
    EA::Raster::SimpleTriangle(pSurface, arrowX, arrowY, 3, o, Color(0xff000000));  // To consider: Make this color configurable or make it it match the text color or something.

    context->restore();
}


IntRect PlatformScrollbar::backButtonRect() const
{
    // Our actual rect will shrink to half the available space when
    // we have < 34 pixels left.  This allows the scrollbar
    // to scale down and function even at tiny sizes.
    if (m_orientation == HorizontalScrollbar)
        return IntRect(x(), y(), cHorizontalButtonWidth, cHorizontalHeight);
    return IntRect(x(), y(), cVerticalWidth, cVerticalButtonHeight);
}

IntRect PlatformScrollbar::forwardButtonRect() const
{
    // Our desired rect is essentially 17x17.
    
    // Our actual rect will shrink to half the available space when
    // we have < 34 pixels left.  This allows the scrollbar
    // to scale down and function even at tiny sizes.
    if (m_orientation == HorizontalScrollbar)
        return IntRect(x() + width() - cHorizontalButtonWidth, y(), cHorizontalButtonWidth, cHorizontalHeight);
    return IntRect(x(), y() + height() - cVerticalButtonHeight, cVerticalWidth, cVerticalButtonHeight);
}

bool PlatformScrollbar::hasThumb() const
{
    return isEnabled() && (m_orientation == HorizontalScrollbar ? width() : height()) >= 2 * cButtonInset + cThumbMinLength + 1;
}

void PlatformScrollbar::splitTrack(const IntRect& trackRect, IntRect& beforeThumbRect, IntRect& thumbRect, IntRect& afterThumbRect) const
{
    // This function won't even get called unless we're big enough to have some combination of these three rects where at least
    // one of them is non-empty.
    int thumbPos = thumbPosition();
    if (m_orientation == HorizontalScrollbar) {
        thumbRect = IntRect(trackRect.x() + thumbPos, trackRect.y() + (trackRect.height() - cThumbHeight) / 2, thumbLength(), cThumbHeight);
        beforeThumbRect = IntRect(trackRect.x(), trackRect.y(), thumbPos, trackRect.height());
        afterThumbRect = IntRect(thumbRect.x() + thumbRect.width(), trackRect.y(), trackRect.right() - thumbRect.right(), trackRect.height());
    } else {
        thumbRect = IntRect(trackRect.x() + (trackRect.width() - cThumbWidth) / 2, trackRect.y() + thumbPos, cThumbWidth, thumbLength());
        beforeThumbRect = IntRect(trackRect.x(), trackRect.y(), trackRect.width(), thumbPos);
        afterThumbRect = IntRect(trackRect.x(), thumbRect.y() + thumbRect.height(), trackRect.width(), trackRect.bottom() - thumbRect.bottom());
    }
}

void PlatformScrollbar::paintThumb(GraphicsContext* context, const IntRect& rect, const IntRect& damageRect) const
{
    if (!damageRect.intersects(rect))
        return;

    context->save();
    paintRectWithBorder(context, rect, Color::gray, Color::lightGray);

    context->restore();
}

int PlatformScrollbar::thumbPosition() const
{
    if (isEnabled())
        return static_cast<int>((float)m_currentPos * (trackLength() - thumbLength()) / (m_totalSize - m_visibleSize));
    return 0;
}

int PlatformScrollbar::trackLength() const
{
    return (m_orientation == HorizontalScrollbar) ? trackRect().width() : trackRect().height();
}

int PlatformScrollbar::thumbLength() const
{
    if (!isEnabled())
        return 0;

    float proportion = (float)(m_visibleSize) / m_totalSize;
    int trackLen = trackLength();
    int length = static_cast<int>(proportion * trackLen);
    int minLength = cThumbMinLength;
    length = max(length, minLength);
    if (length > trackLen)
        length = 0; // Once the thumb is below the track length, it just goes away (to make more room for the track).
    return length;
}

void PlatformScrollbar::SetScrollDrawInfo(EA::WebKit::ScrollbarDrawInfo& sbi, const IntRect& scrollViewDirtyRect)
{
    EA::Raster::Surface* const pSurface = containingWindow();
    ASSERT(pSurface);

    sbi.mpSurface = pSurface;
    IntRectToEARect( scrollViewDirtyRect, sbi.mDirtyRect );

    sbi.mIsVertical = orientation() == VerticalScrollbar;
    switch(getHoveredPart())
    {
    case BackButtonPart:
        sbi.mHoverPart = EA::WebKit::ScrollbarDrawInfo::BackBtn;
        break;
    case ThumbPart:
        sbi.mHoverPart = EA::WebKit::ScrollbarDrawInfo::ThumbBtn;
        break;
    case ForwardButtonPart:
        sbi.mHoverPart = EA::WebKit::ScrollbarDrawInfo::ForwardBtn;
        break;
    default:
        sbi.mHoverPart = EA::WebKit::ScrollbarDrawInfo::None;
        break;
    }
    IntRectToEARect( thumbRect(), sbi.mRectThumb );
    IntRectToEARect( trackRect(), sbi.mRectTrack );
    IntRectToEARect( forwardButtonRect(), sbi.mRectFwdBtn );
    IntRectToEARect( backButtonRect(), sbi.mRectBackBtn );
}

void PlatformScrollbar::paint(GraphicsContext* context, const IntRect& damageRect)
{
    EA::WebKit::ViewNotification* pVN = EA::WebKit::GetViewNotification();
    // EA::Raster::Surface* const pSurface = containingWindow();

    EA::WebKit::ScrollbarDrawInfo sbi;
    SetScrollDrawInfo(sbi, damageRect);

    if(!pVN || !pVN->DrawScrollbar(sbi))
        defaultpaint(context, damageRect);
}

void PlatformScrollbar::defaultpaint(GraphicsContext* context, const IntRect& damageRect)
{
    if (context->updatingControlTints()) {
        invalidate();
        return;
    }

    if (context->paintingDisabled())
        return;

    // Don't paint anything if the scrollbar doesn't intersect the damage rect.
    if (!frameGeometry().intersects(damageRect))
        return;

    IntRect track = trackRect();
    paintTrack(context, track, true, damageRect);

    if (hasButtons()) {
        paintButton(context, backButtonRect(), true, damageRect);
        paintButton(context, forwardButtonRect(), false, damageRect);
    }

     if (hasThumb() && damageRect.intersects(track)) {
        IntRect startTrackRect, thumbRect, endTrackRect;
        splitTrack(track, startTrackRect, thumbRect, endTrackRect);
        paintThumb(context, thumbRect, damageRect);
    }

    Widget::paint(context, damageRect);
}

void PlatformScrollbar::updateThumbPosition()
{
}

void PlatformScrollbar::updateThumbProportion()
{
}

void PlatformScrollbar::setRect(const IntRect& rect)
{
    setFrameGeometry(rect);
    geometryChanged();
}

void PlatformScrollbar::geometryChanged() const
{
    if (!parent())
        return;

    ASSERT(parent()->isFrameView());

    FrameView* frameView = static_cast<FrameView*>(parent());
    IntRect windowRect = IntRect(frameView->contentsToWindow(frameGeometry().location()), frameGeometry().size());

    IntSize sz = frameGeometry().size();
    sz.clampNegativeToZero();

//     GtkAllocation allocation = { windowRect.x(), windowRect.y(), sz.width(), sz.height() };
//     gtk_widget_size_allocate(gtkWidget(), &allocation);
}

void PlatformScrollbar::balValueChanged(BalAdjustment*, PlatformScrollbar* that)
{
//    that->setValue(static_cast<int>(gtk_adjustment_get_value(that->m_adjustment)));
}

int PlatformScrollbar::horizontalScrollbarHeight()
{
    return hSize;
}

int PlatformScrollbar::verticalScrollbarWidth()
{
    return vSize;
}


void PlatformScrollbar::updateViewOnMouseHover()
{
    EA::Raster::Surface* const pSurface = containingWindow();
    EA::WebKit::View* pView = static_cast<EA::WebKit::View*>(pSurface->mpUserData);
    const EA::WebKit::ViewParameters& vPm = pView->GetParameters();
    if(vPm.mbRedrawScrollbarOnCursorHover)
    {
        EA::WebKit::ViewNotification* pVN = EA::WebKit::GetViewNotification();
        if(pVN != NULL)
        {
            GraphicsContext context(pSurface);

            IntRect dmgRect;
            switch ((int)m_hoveredPart)
            {
            case BackButtonPart:
                dmgRect = backButtonRect();
                break;
            case ForwardButtonPart:
                 dmgRect = forwardButtonRect();
                break;
            case ThumbPart:
                 dmgRect = thumbRect();
                break;
            }
            paint(&context, dmgRect);
            EA::WebKit::ViewUpdateInfo vui = { pView,  x(), y(), width(), height() };
            pVN->ViewUpdate(vui);
        }
    }
}

bool PlatformScrollbar::handleMouseMoveEvent(const PlatformMouseEvent& evt)
{
    if (m_pressedPart == ThumbPart) 
    {
        // Drag the thumb.
        int thumbPos = thumbPosition();
        int thumbLen = thumbLength();
        int trackLen = trackLength();
        int maxPos = trackLen - thumbLen;
        int delta = 0;
        if (m_orientation == HorizontalScrollbar)
            delta = convertFromContainingWindow(evt.pos()).x() - m_pressedPos;
        else
            delta = convertFromContainingWindow(evt.pos()).y() - m_pressedPos;

        if (delta > 0)
            // The mouse moved down/right.
            delta = min(maxPos - thumbPos, delta);
        else if (delta < 0)
            // The mouse moved up/left.
            delta = max(-thumbPos, delta);

        if (delta != 0) 
        {
            setValue(static_cast<int>((float)(thumbPos + delta) * (m_totalSize - m_visibleSize) / (trackLen - thumbLen)));
            m_pressedPos += thumbPosition() - thumbPos;
        }

        return true;
    }

    if (m_pressedPart != NoPart)
        m_pressedPos = (m_orientation == HorizontalScrollbar ? convertFromContainingWindow(evt.pos()).x() : convertFromContainingWindow(evt.pos()).y());

    ScrollbarPart part = hitTest(evt);    
    if (part != m_hoveredPart) 
    {
        if (m_pressedPart != NoPart) 
        {
            if (part == m_pressedPart) 
            {
                // The mouse is moving back over the pressed part.  We
                // need to start up the timer action again.
                startTimerIfNeeded(cNormalTimerDelay);
                invalidatePart(m_pressedPart);
            } else if (m_hoveredPart == m_pressedPart) 
            {
                // The mouse is leaving the pressed part.  Kill our timer
                // if needed.
                stopTimerIfNeeded();
                invalidatePart(m_pressedPart);
            }
        } 
        else 
        {
            invalidatePart(part);
            invalidatePart(m_hoveredPart);

        }
        m_hoveredPart = part;

    } 

    updateViewOnMouseHover();
    return true;
}

bool PlatformScrollbar::handleMouseOutEvent(const PlatformMouseEvent& evt)
{
    invalidatePart(m_hoveredPart);
    m_hoveredPart = NoPart;

    updateViewOnMouseHover();

    return true;
}

bool PlatformScrollbar::handleMousePressEvent(const PlatformMouseEvent& evt)
{
    m_pressedPart = hitTest(evt);
    m_pressedPos = (m_orientation == HorizontalScrollbar ? convertFromContainingWindow(evt.pos()).x() : convertFromContainingWindow(evt.pos()).y());
    invalidatePart(m_pressedPart);
    autoscrollPressedPart(cInitialTimerDelay);
    return true;
}

bool PlatformScrollbar::handleMouseReleaseEvent(const PlatformMouseEvent& evt)
{
    invalidatePart(m_pressedPart);
    m_pressedPart = NoPart;
    m_pressedPos = 0;
    stopTimerIfNeeded();

    if (parent() && parent()->isFrameView())
        static_cast<FrameView*>(parent())->frame()->eventHandler()->setMousePressed(evt.button(), false);

    return true;
}

ScrollbarPart PlatformScrollbar::hitTest(const PlatformMouseEvent& evt)
{
    if (!isEnabled())
        return NoPart;

    IntPoint mousePosition = convertFromContainingWindow(evt.pos());
    mousePosition.move(x(), y());

    if (hasButtons()) {
        if (backButtonRect().contains(mousePosition))
            return BackButtonPart;

        if (forwardButtonRect().contains(mousePosition))
            return ForwardButtonPart;
    }

    if (!hasThumb())
        return NoPart;

    IntRect track = trackRect();
    if (track.contains(mousePosition)) {
        IntRect beforeThumbRect, thumbRect, afterThumbRect;
        splitTrack(track, beforeThumbRect, thumbRect, afterThumbRect);
        if (beforeThumbRect.contains(mousePosition))
            return BackTrackPart;
        if (thumbRect.contains(mousePosition))
            return ThumbPart;
        return ForwardTrackPart;
    }

    return NoPart;
}

void PlatformScrollbar::invalidatePart(ScrollbarPart part)
{
    if (part == NoPart)
        return;

    IntRect result;    
    switch (part) {
        case BackButtonPart:
            result = buttonRepaintRect(backButtonRect(), m_orientation, controlSize(), true);
            break;
        case ForwardButtonPart:
            result = buttonRepaintRect(forwardButtonRect(), m_orientation, controlSize(), false);
            break;
        default: {
            IntRect beforeThumbRect, thumbRect, afterThumbRect;
            splitTrack(trackRect(), beforeThumbRect, thumbRect, afterThumbRect);
            if (part == BackTrackPart)
                result = beforeThumbRect;
            else if (part == ForwardTrackPart)
                result = afterThumbRect;
            else
                result = thumbRect;
        }
    }
    result.move(-x(), -y());
    invalidateRect(result);
}

void PlatformScrollbar::autoscrollPressedPart(double delay)
{
    // Don't do anything for the thumb or if nothing was pressed.
    if (m_pressedPart == ThumbPart || m_pressedPart == NoPart)
        return;

    // Handle the track.
    if ((m_pressedPart == BackTrackPart || m_pressedPart == ForwardTrackPart) && thumbUnderMouse()) {
        invalidatePart(m_pressedPart);
        m_hoveredPart = ThumbPart;
        return;
    }

    // Handle the arrows and track.
    if (scroll(pressedPartScrollDirection(), pressedPartScrollGranularity()))
        startTimerIfNeeded(delay);
}

bool PlatformScrollbar::thumbUnderMouse()
{
    // Construct a rect.
    IntRect thumb = thumbRect();
    thumb.move(-x(), -y());
    int begin = (m_orientation == HorizontalScrollbar) ? thumb.x() : thumb.y();
    int end = (m_orientation == HorizontalScrollbar) ? thumb.right() : thumb.bottom();
    return (begin <= m_pressedPos && m_pressedPos < end);
}

IntRect PlatformScrollbar::thumbRect() const
{
    IntRect beforeThumbRect, thumbRect, afterThumbRect;
    splitTrack(trackRect(), beforeThumbRect, thumbRect, afterThumbRect);
    return thumbRect;
}

ScrollDirection PlatformScrollbar::pressedPartScrollDirection()
{
    if (m_orientation == HorizontalScrollbar) {
        if (m_pressedPart == BackButtonPart || m_pressedPart == BackTrackPart)
            return ScrollLeft;
        return ScrollRight;
    } else {
        if (m_pressedPart == BackButtonPart || m_pressedPart == BackTrackPart)
            return ScrollUp;
        return ScrollDown;
    }
}

ScrollGranularity PlatformScrollbar::pressedPartScrollGranularity()
{
    if (m_pressedPart == BackButtonPart || m_pressedPart == ForwardButtonPart)
        return ScrollByLine;
    return ScrollByPage;
}

void PlatformScrollbar::startTimerIfNeeded(double delay)
{
    // Don't do anything for the thumb.
    if (m_pressedPart == ThumbPart)
        return;

    // Handle the track.  We halt track scrolling once the thumb is level
    // with us.
    if ((m_pressedPart == BackTrackPart || m_pressedPart == ForwardTrackPart) && thumbUnderMouse()) {
        invalidatePart(m_pressedPart);
        m_hoveredPart = ThumbPart;
        return;
    }

    // We can't scroll if we've hit the beginning or end.
    ScrollDirection dir = pressedPartScrollDirection();
    if (dir == ScrollUp || dir == ScrollLeft) {
        if (m_currentPos == 0)
            return;
    } else {
        if (m_currentPos == m_totalSize - m_visibleSize)
            return;
    }

    m_scrollTimer.startOneShot(delay);
}

void PlatformScrollbar::stopTimerIfNeeded()
{
    if (m_scrollTimer.isActive())
        m_scrollTimer.stop();
}

void PlatformScrollbar::autoscrollTimerFired(Timer<PlatformScrollbar>*)
{
    autoscrollPressedPart(cNormalTimerDelay);
}




