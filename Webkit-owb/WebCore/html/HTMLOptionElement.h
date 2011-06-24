/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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

#ifndef HTMLOptionElement_h
#define HTMLOptionElement_h

#include "HTMLFormControlElement.h"

namespace WebCore {

class HTMLSelectElement;
class HTMLFormElement;
class MappedAttribute;

class HTMLOptionElement : public HTMLFormControlElement {
    friend class HTMLSelectElement;
    friend class RenderMenuList;

public:
    HTMLOptionElement(Document*, HTMLFormElement* = 0);

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusOptional; }
    virtual int tagPriority() const { return 2; }
    virtual bool checkDTD(const Node* newChild);
    virtual bool isFocusable() const;
    virtual bool rendererIsNeeded(RenderStyle*) { return false; }
    virtual void attach();
    virtual void detach();
    virtual void setRenderStyle(PassRefPtr<RenderStyle>);
    
    virtual const AtomicString& type() const;

    String text() const;
    void setText(const String&, ExceptionCode&);

    int index() const;
    virtual void parseMappedAttribute(MappedAttribute*);

    String value() const;
    void setValue(const String&);

    bool selected() const { return m_selected; }
    void setSelected(bool);
    void setSelectedState(bool);

    HTMLSelectElement* ownerSelectElement() const;

    virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0);

    bool defaultSelected() const;
    void setDefaultSelected(bool);

    String label() const;
    void setLabel(const String&);
    
    String optionText();
    
    virtual bool disabled() const;
    
    virtual void insertedIntoDocument();
    virtual void accessKeyAction(bool);
    
private:
    virtual RenderStyle* nonRendererRenderStyle() const;
    
    String m_value;
    bool m_selected;
    RefPtr<RenderStyle> m_style;
};

} //namespace

#endif
