/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *           (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef JSEventTargetNode_h
#define JSEventTargetNode_h

#include <wtf/FastAllocBase.h>
#include "JSNode.h"
#include "JSEventTargetBase.h"
#include <wtf/AlwaysInline.h>

namespace WebCore {

    class EventTargetNode;
    class Node;

    class JSEventTargetNode : public JSNode {
    public:
        JSEventTargetNode(KJS::JSObject* prototype, Node*);

        void setListener(KJS::ExecState*, const AtomicString& eventType, KJS::JSValue* func) const;
        KJS::JSValue* getListener(const AtomicString& eventType) const;
        virtual void pushEventHandlerScope(KJS::ExecState*, KJS::ScopeChain&) const;

        virtual bool getOwnPropertySlot(KJS::ExecState*, const KJS::Identifier&, KJS::PropertySlot&);
        KJS::JSValue* getValueProperty(KJS::ExecState*, int token) const;
        virtual void put(KJS::ExecState*, const KJS::Identifier&, KJS::JSValue*);
        void putValueProperty(KJS::ExecState*, int token, KJS::JSValue*);

    private:
        JSEventTargetBase<JSEventTargetNode> m_base;
    };

    ALWAYS_INLINE bool JSEventTargetNode::getOwnPropertySlot(KJS::ExecState* exec, const KJS::Identifier& propertyName, KJS::PropertySlot& slot)
    {
        return m_base.getOwnPropertySlot<JSNode>(this, exec, propertyName, slot);
    }

    ALWAYS_INLINE KJS::JSValue* JSEventTargetNode::getValueProperty(KJS::ExecState* exec, int token) const
    {
        return m_base.getValueProperty(this, exec, token);
    }

    ALWAYS_INLINE void JSEventTargetNode::put(KJS::ExecState* exec, const KJS::Identifier& propertyName, KJS::JSValue* value)
    {
        m_base.put<JSNode>(this, exec, propertyName, value);
    }

    ALWAYS_INLINE void JSEventTargetNode::putValueProperty(KJS::ExecState* exec, int token, KJS::JSValue* value)
    {
        m_base.putValueProperty(this, exec, token, value);
    }


    struct JSEventTargetPrototypeInformation: public WTF::FastAllocBase {
        static const char* prototypeClassName()
        {
            return "EventTargetNodePrototype";
        }

        static const char* prototypeIdentifier()
        {
            return "[[EventTargetNode.prototype]]";
        }
    };

    typedef JSEventTargetPrototype<JSNodePrototype, JSEventTargetPrototypeInformation> JSEventTargetNodePrototype;
    EventTargetNode* toEventTargetNode(KJS::JSValue*);

} // namespace WebCore

#endif // JSEventTargetNode_h
