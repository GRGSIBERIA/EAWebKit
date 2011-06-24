/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
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
#include "JSInspectedObjectWrapper.h"

#include "JSInspectorCallbackWrapper.h"

using namespace KJS;

namespace WebCore {

typedef HashMap<JSObject*, JSInspectedObjectWrapper*> WrapperMap;
typedef HashMap<JSGlobalObject*, WrapperMap*> GlobalObjectWrapperMap;

//+ 4/28/09 CSidhall - Extracted from wrappers() for leak fix on exit.
// Original code:

//static GlobalObjectWrapperMap& wrappers()
//{
//    static GlobalObjectWrapperMap map;
//    return map;
//}

// New code:
static GlobalObjectWrapperMap* spMap = NULL;
void JSInspectorCallbackWrapper::staticFinalize()
{
    if(spMap) {
        deleteAllValues(*spMap);
        delete (spMap);
        spMap = NULL;
    }

    // CS - Might need to verify that s_info is cleaned ok on exit
}

static GlobalObjectWrapperMap& wrappers()
{
    if(!spMap) {
        spMap = new (GlobalObjectWrapperMap);   
        ASSERT(spMap);
    }        
    return *spMap;
}
//- CS

const ClassInfo JSInspectedObjectWrapper::s_info = { "JSInspectedObjectWrapper", &JSQuarantinedObjectWrapper::s_info, 0, 0 };

JSValue* JSInspectedObjectWrapper::wrap(ExecState* unwrappedExec, JSValue* unwrappedValue)
{
    if (!unwrappedValue->isObject())
        return unwrappedValue;

    JSObject* unwrappedObject = static_cast<JSObject*>(unwrappedValue);

    if (unwrappedObject->inherits(&JSInspectedObjectWrapper::s_info))
        return unwrappedObject;

    if (WrapperMap* wrapperMap = wrappers().get(unwrappedExec->dynamicGlobalObject()))
        if (JSInspectedObjectWrapper* wrapper = wrapperMap->get(unwrappedObject))
            return wrapper;

    JSValue* prototype = unwrappedObject->prototype();
    JSValue* wrappedPrototype = prototype ? wrap(unwrappedExec, prototype) : 0;

    return new (unwrappedExec) JSInspectedObjectWrapper(unwrappedExec, unwrappedObject, wrappedPrototype);
}

JSInspectedObjectWrapper::JSInspectedObjectWrapper(ExecState* unwrappedExec, JSObject* unwrappedObject, JSValue* wrappedPrototype)
    : JSQuarantinedObjectWrapper(unwrappedExec, unwrappedObject, wrappedPrototype)
{
    WrapperMap* wrapperMap = wrappers().get(unwrappedGlobalObject());
    if (!wrapperMap) {
        wrapperMap = new WrapperMap;
        wrappers().set(unwrappedGlobalObject(), wrapperMap);
    }

    ASSERT(!wrapperMap->contains(unwrappedObject));
    wrapperMap->set(unwrappedObject, this);
}

JSInspectedObjectWrapper::~JSInspectedObjectWrapper()
{
    ASSERT(wrappers().contains(unwrappedGlobalObject()));
    WrapperMap* wrapperMap = wrappers().get(unwrappedGlobalObject());

    ASSERT(wrapperMap->contains(unwrappedObject()));
    wrapperMap->remove(unwrappedObject());

    if (wrapperMap->isEmpty()) {
        wrappers().remove(unwrappedGlobalObject());
        delete wrapperMap;
    }
}

JSValue* JSInspectedObjectWrapper::prepareIncomingValue(ExecState*, JSValue* value) const
{
    // The Inspector is only allowed to pass primitive values and wrapped objects to objects from the inspected page.

    if (!value->isObject())
        return value;

    JSQuarantinedObjectWrapper* wrapper = asWrapper(value);
    ASSERT_WITH_MESSAGE(wrapper, "Objects passed to JSInspectedObjectWrapper must be wrapped");
    if (!wrapper)
        return jsUndefined();

    if (wrapper->allowsUnwrappedAccessFrom(unwrappedExecState())) {
        ASSERT_WITH_MESSAGE(wrapper->inherits(&s_info), "A wrapper contains an object from the inspected page but is not a JSInspectedObjectWrapper");
        if (!wrapper->inherits(&s_info))
            return jsUndefined();

        // Return the unwrapped object so the inspected page never sees one of its own objects in wrapped form.
        return wrapper->unwrappedObject();
    }

    ASSERT_WITH_MESSAGE(wrapper->inherits(&JSInspectorCallbackWrapper::s_info), "A wrapper that was not from the inspected page and is not an Inspector callback was passed to a JSInspectedObjectWrapper");
    if (!wrapper->inherits(&JSInspectorCallbackWrapper::s_info))
        return jsUndefined();

    return wrapper;
}

} // namespace WebCore
