/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "JSPluginElementFunctions.h"

#if USE(JAVASCRIPTCORE_BINDINGS)

#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLDocument.h"
#include "HTMLNames.h"
#include "HTMLPlugInElement.h"
#include "JSHTMLElement.h"
#include "ScriptController.h"
#include "runtime.h"
#include "runtime_object.h"

#endif

using namespace KJS;

namespace WebCore {

#if !USE(JAVASCRIPTCORE_BINDINGS)

JSValue* runtimeObjectGetter(ExecState*, const Identifier&, const PropertySlot&)
{
    return jsUndefined();
}

JSValue* runtimeObjectPropertyGetter(ExecState*, const Identifier&, const PropertySlot&)
{
    return jsUndefined();
}

bool runtimeObjectCustomGetOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&, JSHTMLElement*)
{
    return false;
}

bool runtimeObjectCustomPut(ExecState*, const Identifier&, JSValue*, HTMLElement*)
{
    return false;
}

CallType runtimeObjectGetCallData(HTMLElement*, CallData&)
{
    return CallTypeNone;
}

#else

using namespace Bindings;
using namespace HTMLNames;

// Runtime object support code for JSHTMLAppletElement, JSHTMLEmbedElement and JSHTMLObjectElement.

static Instance* pluginInstance(Node* node)
{
    if (!node)
        return 0;
    if (!(node->hasTagName(objectTag) || node->hasTagName(embedTag) || node->hasTagName(appletTag)))
        return 0;
    HTMLPlugInElement* plugInElement = static_cast<HTMLPlugInElement*>(node);
    Instance* instance = plugInElement->getInstance();
    if (!instance || !instance->rootObject())
        return 0;
    return instance;
}

static RuntimeObjectImp* getRuntimeObject(ExecState* exec, Node* node)
{
    Instance* instance = pluginInstance(node);
    if (!instance)
        return 0;
    return KJS::Bindings::Instance::createRuntimeObject(exec, instance);
}

JSValue* runtimeObjectGetter(ExecState* exec, const Identifier& propertyName, const PropertySlot& slot)
{
    JSHTMLElement* thisObj = static_cast<JSHTMLElement*>(slot.slotBase());
    HTMLElement* element = static_cast<HTMLElement*>(thisObj->impl());
    RuntimeObjectImp* runtimeObject = getRuntimeObject(exec, element);
    return runtimeObject ? runtimeObject : jsUndefined();
}

JSValue* runtimeObjectPropertyGetter(ExecState* exec, const Identifier& propertyName, const PropertySlot& slot)
{
    JSHTMLElement* thisObj = static_cast<JSHTMLElement*>(slot.slotBase());
    HTMLElement* element = static_cast<HTMLElement*>(thisObj->impl());
    RuntimeObjectImp* runtimeObject = getRuntimeObject(exec, element);
    if (!runtimeObject)
        return jsUndefined();
    return runtimeObject->get(exec, propertyName);
}

bool runtimeObjectCustomGetOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot, JSHTMLElement* element)
{
    RuntimeObjectImp* runtimeObject = getRuntimeObject(exec, element->impl());
    if (!runtimeObject)
        return false;
    if (!runtimeObject->hasProperty(exec, propertyName))
        return false;
    slot.setCustom(element, runtimeObjectPropertyGetter);
    return true;
}

bool runtimeObjectCustomPut(ExecState* exec, const Identifier& propertyName, JSValue* value, HTMLElement* element)
{
    RuntimeObjectImp* runtimeObject = getRuntimeObject(exec, element);
    if (!runtimeObject)
        return 0;
    if (!runtimeObject->hasProperty(exec, propertyName))
        return false;
    runtimeObject->put(exec, propertyName, value);
    return true;
}

static JSValue* callPlugin(ExecState* exec, JSObject* function, JSValue*, const ArgList& args)
{
    Instance* instance = pluginInstance(static_cast<JSHTMLElement*>(function)->impl());
    instance->begin();
    JSValue* result = instance->invokeDefaultMethod(exec, args);
    instance->end();
    return result;
}

CallType runtimeObjectGetCallData(HTMLElement* element, CallData& callData)
{
    Instance* instance = pluginInstance(element);
    if (!instance || !instance->supportsInvokeDefaultMethod())
        return CallTypeNone;
    callData.native.function = callPlugin;
    return CallTypeNative;
}

#endif

} // namespace WebCore
