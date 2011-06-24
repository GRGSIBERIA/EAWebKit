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

#ifndef JSEventTargetBase_h
#define JSEventTargetBase_h

#include <wtf/FastAllocBase.h>
#include "Document.h"
#include "Event.h"
#include "JSEvent.h"

namespace KJS {

    extern const struct HashTable JSEventTargetPropertiesTable;
    extern const struct HashTable JSEventTargetPrototypeTable;

}

namespace WebCore {

    class AtomicString;
    class EventTarget;

    // Event target properties (shared across all JSEventTarget* classes)
    struct JSEventTargetProperties: public WTF::FastAllocBase {
        enum {
            AddEventListener, RemoveEventListener, DispatchEvent,
            OnAbort, OnBlur, OnChange, OnClick, OnContextMenu, OnDblClick, OnError,
            OnDragEnter, OnDragOver, OnDragLeave, OnDrop, OnDragStart, OnDrag, OnDragEnd,
            OnBeforeCut, OnCut, OnBeforeCopy, OnCopy, OnBeforePaste, OnPaste, OnSelectStart,
            OnFocus, OnInput, OnKeyDown, OnKeyPress, OnKeyUp, OnLoad, OnMouseDown,
            OnMouseMove, OnMouseOut, OnMouseOver, OnMouseUp, OnMouseWheel, OnReset,
            OnResize, OnScroll, OnSearch, OnSelect, OnSubmit, OnUnload
        };
    };

    // Helper function for getValueProperty/putValueProperty
    const AtomicString& eventNameForPropertyToken(int token);

    template<class JSEventTarget>
    class JSEventTargetBase: public WTF::FastAllocBase {
    public:
        JSEventTargetBase() { }

        KJS::JSValue* getValueProperty(const JSEventTarget* owner, KJS::ExecState* exec, int token) const
        {
            const AtomicString& eventName = eventNameForPropertyToken(token);
            if (!eventName.isEmpty())
                return owner->getListener(eventName);

            return KJS::jsUndefined();
        }

        void putValueProperty(const JSEventTarget* owner, KJS::ExecState* exec, int token, KJS::JSValue* value)
        {
            const AtomicString& eventName = eventNameForPropertyToken(token);
            if (!eventName.isEmpty())
                owner->setListener(exec, eventName, value);
        }

    private:
        friend class JSEventTargetNode;
        friend class JSEventTargetSVGElementInstance;

        template<class JSParent>
        bool getOwnPropertySlot(JSEventTarget* owner, KJS::ExecState* exec, const KJS::Identifier& propertyName, KJS::PropertySlot& slot)
        {
            return KJS::getStaticValueSlot<JSEventTarget, JSParent>(exec, &KJS::JSEventTargetPropertiesTable, owner, propertyName, slot);
        }

        template<class JSParent>
        void put(JSEventTarget* owner, KJS::ExecState* exec, const KJS::Identifier& propertyName, KJS::JSValue* value)
        {
            KJS::lookupPut<JSEventTarget, JSParent>(exec, propertyName, value, &KJS::JSEventTargetPropertiesTable, owner);
        }
    };

    // This class is a modified version of the code the KJS_DEFINE_PROTOTYPE_WITH_PROTOTYPE
    // and KJS_IMPLEMENT_PROTOTYPE macros produce - the idea is that classes like JSEventTargetNode
    // and JSEventTargetSVGElementInstance can share a single prototype just differing in the
    // naming "EventTargetNodePrototype" vs "EventTargetSVGElementInstancePrototype". Above mentioned
    // macros force the existance of several prototype tables for each of the classes - avoid that.
    template<class JSEventTargetPrototypeParent, class JSEventTargetPrototypeInformation>
    class JSEventTargetPrototype : public KJS::JSObject {
    public:
        JSEventTargetPrototype(KJS::ExecState* exec)
            : KJS::JSObject(JSEventTargetPrototypeParent::self(exec))
        {
        }

        // 3/26/09 CSidhall - Extracted static from self() to get access for memory leak
        static KJS::Identifier* m_pPrototypeName; 
        static void staticFinalize()
        {
            if(m_pPrototypeName) {
                delete m_pPrototypeName;
                m_pPrototypeName = NULL;
            }
        }

        static KJS::JSObject* self(KJS::ExecState* exec)
        {
             if(!m_pPrototypeName)
                 m_pPrototypeName = new KJS::Identifier(exec, JSEventTargetPrototypeInformation::prototypeClassName());
            
            // CS - Old code:
            //static KJS::Identifier* prototypeName = new KJS::Identifier(exec, JSEventTargetPrototypeInformation::prototypeClassName());

            KJS::JSGlobalObject* globalObject = exec->lexicalGlobalObject();
            if (KJS::JSValue* objectValue = globalObject->getDirect(*m_pPrototypeName)) {
                ASSERT(objectValue->isObject());
                return static_cast<KJS::JSObject*>(objectValue);
            }

            KJS::JSObject* newObject = new (exec) JSEventTargetPrototype<JSEventTargetPrototypeParent, JSEventTargetPrototypeInformation>(exec);
            globalObject->putDirect(*m_pPrototypeName, newObject, KJS::DontEnum);
            return newObject;
        }

        bool getOwnPropertySlot(KJS::ExecState* exec, const KJS::Identifier& propertyName, KJS::PropertySlot& slot)
        {
            return KJS::getStaticFunctionSlot<KJS::JSObject>(exec, &KJS::JSEventTargetPrototypeTable, this, propertyName, slot);
        }

        virtual const KJS::ClassInfo* classInfo() const
        {
            static const KJS::ClassInfo s_classInfo = { JSEventTargetPrototypeInformation::prototypeClassName(), 0, &KJS::JSEventTargetPrototypeTable, 0 };
            return &s_classInfo;
        }
    };

    KJS::JSValue* toJS(KJS::ExecState*, EventTarget*);

} // namespace WebCore

#endif // JSEventTargetBase_h
