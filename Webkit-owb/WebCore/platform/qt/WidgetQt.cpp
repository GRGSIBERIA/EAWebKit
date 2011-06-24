/*
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 George Stiakos <staikos@kde.org>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Holger Hans Peter Freyther
 *
 * All rights reserved.
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

#include "Cursor.h"
#include "Font.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "IntRect.h"
#include "RenderObject.h"
#include "ScrollView.h"
#include "Widget.h"
#include "WidgetClient.h"
#include "PlatformScrollBar.h"
#include "NotImplemented.h"

#include "qwebframe.h"
#include "qwebframe_p.h"
#include "qwebpage.h"

#include <QCoreApplication>
#include <QPainter>
#include <QPaintEngine>

#include <QDebug>

namespace WebCore {

#include <wtf/FastAllocBase.h>
struct WidgetPrivate: public WTF::FastAllocBase
{
    WidgetPrivate()
        : m_client(0)
        , enabled(true)
        , suppressInvalidation(false)
        , m_widget(0)
        , isNPAPIPlugin(0)
        , m_parentScrollView(0) { }
    ~WidgetPrivate() {}

    WidgetClient* m_client;

    bool enabled;
    bool suppressInvalidation;
    bool isNPAPIPlugin;
    IntRect m_geometry;
    QWidget *m_widget; //for plugins
    ScrollView *m_parentScrollView;
};

Widget::Widget()
    : data(new WidgetPrivate())
{
}

Widget::~Widget()
{
    Q_ASSERT(!parent());
    delete data;
    data = 0;
}

void Widget::setClient(WidgetClient* c)
{
    data->m_client = c;
}

WidgetClient* Widget::client() const
{
    return data->m_client;
}

IntRect Widget::frameGeometry() const
{
    return data->m_geometry;
}

void Widget::setFrameGeometry(const IntRect& r)
{
    data->m_geometry = r;
    if (data->m_widget)
        data->m_widget->setGeometry(convertToContainingWindow(IntRect(0, 0, r.width(), r.height())));
}

void Widget::setFocus()
{
}

void Widget::setCursor(const Cursor& cursor)
{
#ifndef QT_NO_CURSOR
    if (QWidget* widget = containingWindow())
        QCoreApplication::postEvent(widget, new SetCursorEvent(cursor.impl()));
#endif
}

void Widget::show()
{
    if (data->m_widget)
        data->m_widget->show();
}

void Widget::hide()
{
    if (data->m_widget)
        data->m_widget->hide();
}

QWidget* Widget::nativeWidget() const
{
    return data->m_widget;
}

void Widget::setNativeWidget(QWidget *widget)
{
    data->m_widget = widget;
}

bool Widget::isNPAPIPlugin() const
{
    return data->isNPAPIPlugin;
}

void Widget::setIsNPAPIPlugin(bool is)
{
    data->isNPAPIPlugin = is;
}

void Widget::paint(GraphicsContext *, const IntRect &rect)
{
}

bool Widget::isEnabled() const
{
    if (data->m_widget)
        return data->m_widget->isEnabled();
    return data->enabled;
}

void Widget::setEnabled(bool e)
{
    if (data->m_widget)
        data->m_widget->setEnabled(e);

    if (e != data->enabled) {
        data->enabled = e;
        invalidate();
    }
}

void Widget::setIsSelected(bool)
{
    notImplemented();
}

bool Widget::suppressInvalidation() const
{
    return data->suppressInvalidation;
}

void Widget::setSuppressInvalidation(bool suppress)
{
    data->suppressInvalidation = suppress;
}

void Widget::invalidate()
{
    invalidateRect(IntRect(0, 0, width(), height()));
}

void Widget::invalidateRect(const IntRect& r)
{
    if (data->suppressInvalidation)
        return;

    if (data->m_widget) { //plugins
        data->m_widget->update(r);
        return;
    }

    if (!parent()) {
        if (isFrameView())
            static_cast<FrameView*>(this)->addToDirtyRegion(r);
        return;
    }

    // Get the root widget.
    ScrollView* outermostView = topLevel();
    if (!outermostView)
        return;

    IntRect windowRect = convertToContainingWindow(r);

    // Get our clip rect and intersect with it to ensure we don't invalidate too much.
    IntRect clipRect = windowClipRect();
    windowRect.intersect(clipRect);

    outermostView->addToDirtyRegion(windowRect);
}

void Widget::removeFromParent()
{
    if (parent())
        parent()->removeChild(this);
}

void Widget::setParent(ScrollView* sv)
{
    data->m_parentScrollView = sv;
}

ScrollView* Widget::parent() const
{
    return data->m_parentScrollView;
}

ScrollView* Widget::topLevel() const
{
    if (!data->m_parentScrollView)
        return isFrameView() ? const_cast<ScrollView*>(static_cast<const ScrollView*>(this)) : 0;
    ScrollView* topLevel = data->m_parentScrollView;
    while (topLevel->data->m_parentScrollView)
        topLevel = topLevel->data->m_parentScrollView;
    return topLevel;
}

QWidget *Widget::containingWindow() const
{
    ScrollView *topLevel = this->topLevel();
    if (!topLevel)
        return 0;

    if (!topLevel->isFrameView())
        return data->m_widget;

    QWebFrame* frame = QWebFramePrivate::kit(static_cast<FrameView*>(topLevel)->frame());
    QWidget* view = frame->page()->view();

    return view ? view : data->m_widget;
}


void Widget::geometryChanged() const
{
}

IntPoint Widget::convertToContainingWindow(const IntPoint& point) const
{
    IntPoint windowPoint = point;
    for (const Widget *parentWidget = parent(), *childWidget = this;
         parentWidget;
         childWidget = parentWidget, parentWidget = parentWidget->parent()) {
        IntPoint oldPoint = windowPoint;
        windowPoint = parentWidget->convertChildToSelf(childWidget, oldPoint);
    }
    return windowPoint;
}

IntPoint Widget::convertFromContainingWindow(const IntPoint& point) const
{
    IntPoint widgetPoint = point;
    for (const Widget *parentWidget = parent(), *childWidget = this;
         parentWidget;
         childWidget = parentWidget, parentWidget = parentWidget->parent()) {
        IntPoint oldPoint = widgetPoint;
        widgetPoint = parentWidget->convertSelfToChild(childWidget, oldPoint);
    }
    return widgetPoint;
}

IntRect Widget::convertToContainingWindow(const IntRect& rect) const
{
    IntRect convertedRect = rect;
    convertedRect.setLocation(convertToContainingWindow(convertedRect.location()));
    return convertedRect;
}

IntPoint Widget::convertChildToSelf(const Widget* child, const IntPoint& point) const
{
    return IntPoint(point.x() + child->x(), point.y() + child->y());
}

IntPoint Widget::convertSelfToChild(const Widget* child, const IntPoint& point) const
{
    return IntPoint(point.x() - child->x(), point.y() - child->y());
}

}

// vim: ts=4 sw=4 et
