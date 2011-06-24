/*
 * Copyright (C) 2006 Apple Computer, Inc.
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

#ifndef PopupMenuClient_h
#define PopupMenuClient_h

#include <wtf/FastAllocBase.h>
namespace WebCore {

class Color;
class Document;
class FontSelector;
class String;
class RenderStyle;

class PopupMenuClient: public WTF::FastAllocBase {
public:
    virtual ~PopupMenuClient() {}
    virtual void valueChanged(unsigned listIndex, bool fireEvents = true) = 0;

    virtual String itemText(unsigned listIndex) const = 0;
    virtual bool itemIsEnabled(unsigned listIndex) const = 0;
    virtual Color itemBackgroundColor(unsigned listIndex) const = 0;
    virtual RenderStyle* itemStyle(unsigned listIndex) const = 0;
    virtual RenderStyle* clientStyle() const = 0;
    virtual Document* clientDocument() const = 0;
    virtual int clientInsetLeft() const = 0;
    virtual int clientInsetRight() const = 0;
    virtual int clientPaddingLeft() const = 0;
    virtual int clientPaddingRight() const = 0;
    virtual int listSize() const = 0;
    virtual int selectedIndex() const = 0;
    virtual void hidePopup() = 0;
    virtual bool itemIsSeparator(unsigned listIndex) const = 0;
    virtual bool itemIsLabel(unsigned listIndex) const = 0;
    virtual bool itemIsSelected(unsigned listIndex) const = 0;
    virtual bool shouldPopOver() const = 0;
    virtual bool valueShouldChangeOnHotTrack() const = 0;
    virtual void setTextFromItem(unsigned listIndex) = 0;
    virtual FontSelector* fontSelector() const = 0;
};

}

#endif
