/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#ifndef ContainerNode_h
#define ContainerNode_h

#include "EventTargetNode.h"

namespace WebCore {
    
typedef void (*NodeCallback)(Node*);

class ContainerNode : public EventTargetNode {
public:
    ContainerNode(Document*);
    virtual ~ContainerNode();

    Node* firstChild() const { return m_firstChild; }
    Node* lastChild() const { return m_lastChild; }

    virtual bool insertBefore(PassRefPtr<Node> newChild, Node* refChild, ExceptionCode&, bool shouldLazyAttach = false);
    virtual bool replaceChild(PassRefPtr<Node> newChild, Node* oldChild, ExceptionCode&, bool shouldLazyAttach = false);
    virtual bool removeChild(Node* child, ExceptionCode&);
    virtual bool appendChild(PassRefPtr<Node> newChild, ExceptionCode&, bool shouldLazyAttach = false);

    virtual ContainerNode* addChild(PassRefPtr<Node>);
    bool hasChildNodes() const { return m_firstChild; }
    virtual void attach();
    virtual void detach();
    virtual void willRemove();
    virtual IntRect getRect() const;
    virtual void setFocus(bool = true);
    virtual void setActive(bool active = true, bool pause = false);
    virtual void setHovered(bool = true);
    virtual unsigned childNodeCount() const;
    virtual Node* childNode(unsigned index) const;

    virtual void insertedIntoDocument();
    virtual void removedFromDocument();
    virtual void insertedIntoTree(bool deep);
    virtual void removedFromTree(bool deep);
    virtual void childrenChanged(bool createdByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0);

    virtual bool removeChildren();

    void removeAllChildren();

    void cloneChildNodes(ContainerNode* clone);
    static void staticFinalize();   // 3/26/09 - CS Added for static leak

protected:
    static void queuePostAttachCallback(NodeCallback, Node*);
    static void suspendPostAttachCallbacks();
    static void resumePostAttachCallbacks();

    void setFirstChild(Node* child) { m_firstChild = child; }
    void setLastChild(Node* child) { m_lastChild = child; }
    
private:
    static void dispatchPostAttachCallbacks();

    virtual Node* virtualFirstChild() const;
    virtual Node* virtualLastChild() const;
    
    static void addChildNodesToDeletionQueue(Node*& head, Node*& tail, ContainerNode*);

    bool getUpperLeftCorner(int& x, int& y) const;
    bool getLowerRightCorner(int& x, int& y) const;

    Node* m_firstChild;
    Node* m_lastChild;
};

} // namespace WebCore

#endif // ContainerNode_h
