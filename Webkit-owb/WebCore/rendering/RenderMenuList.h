/*
 * This file is part of the select element renderer in WebCore.
 *
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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

#ifndef RenderMenuList_h
#define RenderMenuList_h

#include "RenderFlexibleBox.h"
#include "PopupMenuClient.h"

#if PLATFORM(MAC)
#define POPUP_MENU_PULLS_DOWN 0
#else
#define POPUP_MENU_PULLS_DOWN 1
#endif

namespace WebCore {

class HTMLSelectElement;
class PopupMenu;

class RenderMenuList : public RenderFlexibleBox, private PopupMenuClient {
public:
    RenderMenuList(HTMLSelectElement*);
    ~RenderMenuList();
    
    HTMLSelectElement* selectElement();

    virtual bool isMenuList() const { return true; }

    virtual void addChild(RenderObject* newChild, RenderObject* beforeChild = 0);
    virtual void removeChild(RenderObject*);
    virtual bool createsAnonymousWrapper() const { return true; }
    virtual bool canHaveChildren() const { return false; }

    virtual void setStyle(RenderStyle*);
    virtual void updateFromElement();

    virtual bool hasControlClip() const { return true; }
    virtual IntRect controlClipRect(int tx, int ty) const;

    virtual const char* renderName() const { return "RenderMenuList"; }

    virtual void calcPrefWidths();

    bool popupIsVisible() const { return m_popupIsVisible; }
    void showPopup();
    void hidePopup();

    void setOptionsChanged(bool changed) { m_optionsChanged = changed; }

    const PopupMenu* GetPopup() { return m_popup.get(); }           // Added by EA
    virtual void paint(PaintInfo& paintInfo, int tx, int ty);       // Added by EA

    String text() const;
    
private:
    // PopupMenuClient methods
    virtual String itemText(unsigned listIndex) const;
    virtual bool itemIsEnabled(unsigned listIndex) const;
    virtual Color itemBackgroundColor(unsigned listIndex) const;
    virtual RenderStyle* itemStyle(unsigned listIndex) const;
    virtual RenderStyle* clientStyle() const;
    virtual Document* clientDocument() const;
    virtual RenderBlock* clientRenderBlock();                       // Added by EA
    virtual int clientInsetLeft() const;
    virtual int clientInsetRight() const;
    virtual int clientPaddingLeft() const;
    virtual int clientPaddingRight() const;
    virtual int listSize() const;
    virtual int selectedIndex() const;
    virtual bool itemIsSeparator(unsigned listIndex) const;
    virtual bool itemIsLabel(unsigned listIndex) const;
    virtual bool itemIsSelected(unsigned listIndex) const;
    virtual void setTextFromItem(unsigned listIndex);
    virtual bool valueShouldChangeOnHotTrack() const { return true; }
    virtual bool shouldPopOver() const { return !POPUP_MENU_PULLS_DOWN; }
    virtual void valueChanged(unsigned listIndex, bool fireOnChange = true);
    virtual FontSelector* fontSelector() const;

    virtual bool hasLineIfEmpty() const { return true; }

    void createInnerBlock();
    void adjustInnerStyle();
    void setText(const String&);
    void setTextFromOption(int optionIndex);
    void updateOptionsWidth();

    RenderText* m_buttonText;
    RenderBlock* m_innerBlock;

    bool m_optionsChanged;
    int m_optionsWidth;

    RefPtr<PopupMenu> m_popup;
    bool m_popupIsVisible;
};

}

#endif
