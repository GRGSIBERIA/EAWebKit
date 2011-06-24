/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2000 Frederik Holljen (frederik.holljen@hig.no)
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004, 2008 Apple Inc. All rights reserved.
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

#ifndef TreeWalker_h
#define TreeWalker_h

#include "Traversal.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

    typedef int ExceptionCode;

    class TreeWalker : public RefCounted<TreeWalker>, public Traversal {
    public:
        static PassRefPtr<TreeWalker> create(PassRefPtr<Node> rootNode, unsigned whatToShow, PassRefPtr<NodeFilter> filter, bool expandEntityReferences)
        {
            return adoptRef(new TreeWalker(rootNode, whatToShow, filter, expandEntityReferences));
        }                            

        Node* currentNode() const { return m_current.get(); }
        void setCurrentNode(PassRefPtr<Node>, ExceptionCode&);

        Node* parentNode(KJS::JSValue*& exception);
        Node* firstChild(KJS::JSValue*& exception);
        Node* lastChild(KJS::JSValue*& exception);
        Node* previousSibling(KJS::JSValue*& exception);
        Node* nextSibling(KJS::JSValue*& exception);
        Node* previousNode(KJS::JSValue*& exception);
        Node* nextNode(KJS::JSValue*& exception);

        // For non-JS bindings. Silently ignores the JavaScript exception if any.
        Node* parentNode() { KJS::JSValue* exception; return parentNode(exception); }
        Node* firstChild() { KJS::JSValue* exception; return firstChild(exception); }
        Node* lastChild() { KJS::JSValue* exception; return lastChild(exception); }
        Node* previousSibling() { KJS::JSValue* exception; return previousSibling(exception); }
        Node* nextSibling() { KJS::JSValue* exception; return nextSibling(exception); }
        Node* previousNode() { KJS::JSValue* exception; return previousNode(exception); }
        Node* nextNode() { KJS::JSValue* exception; return nextNode(exception); }

    private:
        TreeWalker(PassRefPtr<Node>, unsigned whatToShow, PassRefPtr<NodeFilter>, bool expandEntityReferences);
        
        Node* setCurrent(PassRefPtr<Node>);

        RefPtr<Node> m_current;
    };

} // namespace WebCore

#endif // TreeWalker_h
