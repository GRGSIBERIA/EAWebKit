// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include "JSObjectRef.h"
 
#include "APICast.h"
#include "FunctionConstructor.h"
#include "JSCallbackConstructor.h"
#include "JSCallbackFunction.h"
#include "JSCallbackObject.h"
#include "JSClassRef.h"
#include "JSFunction.h"
#include "JSGlobalObject.h"
#include "JSObject.h"
#include "JSString.h"
#include "JSValueRef.h"
#include "ObjectPrototype.h"
#include "PropertyNameArray.h"
#include "identifier.h"
#include <wtf/Platform.h>

using namespace KJS;

JSClassRef JSClassCreate(const JSClassDefinition* definition)
{
    JSLock lock;
    RefPtr<OpaqueJSClass> jsClass = (definition->attributes & kJSClassAttributeNoAutomaticPrototype)
        ? OpaqueJSClass::createNoAutomaticPrototype(definition)
        : OpaqueJSClass::create(definition);
    
    return jsClass.release().releaseRef();
}

JSClassRef JSClassRetain(JSClassRef jsClass)
{
    JSLock lock;
    jsClass->ref();
    return jsClass;
}

void JSClassRelease(JSClassRef jsClass)
{
    JSLock lock;
    jsClass->deref();
}

JSObjectRef JSObjectMake(JSContextRef ctx, JSClassRef jsClass, void* data)
{
    JSLock lock;
    ExecState* exec = toJS(ctx);

    if (!jsClass)
        return toRef(new (exec) JSObject(exec->lexicalGlobalObject()->objectPrototype())); // slightly more efficient

    JSValue* jsPrototype = jsClass->prototype(ctx);
    if (!jsPrototype)
        jsPrototype = exec->lexicalGlobalObject()->objectPrototype();

    return toRef(new (exec) JSCallbackObject<JSObject>(exec, jsClass, jsPrototype, data));
}

JSObjectRef JSObjectMakeFunctionWithCallback(JSContextRef ctx, JSStringRef name, JSObjectCallAsFunctionCallback callAsFunction)
{
    JSLock lock;
    ExecState* exec = toJS(ctx);
    Identifier nameID = name ? Identifier(exec, toJS(name)) : Identifier(exec, "anonymous");
    
    return toRef(new (exec) JSCallbackFunction(exec, callAsFunction, nameID));
}

JSObjectRef JSObjectMakeConstructor(JSContextRef ctx, JSClassRef jsClass, JSObjectCallAsConstructorCallback callAsConstructor)
{
    JSLock lock;
    ExecState* exec = toJS(ctx);
    
    JSValue* jsPrototype = jsClass 
        ? jsClass->prototype(ctx)
        : exec->dynamicGlobalObject()->objectPrototype();
    
    JSCallbackConstructor* constructor = new (exec) JSCallbackConstructor(exec, jsClass, callAsConstructor);
    constructor->putDirect(exec->propertyNames().prototype, jsPrototype, DontEnum | DontDelete | ReadOnly);
    return toRef(constructor);
}

JSObjectRef JSObjectMakeFunction(JSContextRef ctx, JSStringRef name, unsigned parameterCount, const JSStringRef parameterNames[], JSStringRef body, JSStringRef sourceURL, int startingLineNumber, JSValueRef* exception)
{
    JSLock lock;
    
    ExecState* exec = toJS(ctx);
    UString::Rep* bodyRep = toJS(body);
    UString::Rep* sourceURLRep = sourceURL ? toJS(sourceURL) : &UString::Rep::null;
    
    Identifier nameID = name ? Identifier(exec, toJS(name)) : Identifier(exec, "anonymous");
    
    ArgList args;
    for (unsigned i = 0; i < parameterCount; i++)
        args.append(jsString(exec, UString(toJS(parameterNames[i]))));
    args.append(jsString(exec, UString(bodyRep)));

    JSObject* result = constructFunction(exec, args, nameID, UString(sourceURLRep), startingLineNumber);
    if (exec->hadException()) {
        if (exception)
            *exception = toRef(exec->exception());
        exec->clearException();
        result = 0;
    }
    return toRef(result);
}

JSValueRef JSObjectGetPrototype(JSContextRef, JSObjectRef object)
{
    JSObject* jsObject = toJS(object);
    return toRef(jsObject->prototype());
}

void JSObjectSetPrototype(JSContextRef, JSObjectRef object, JSValueRef value)
{
    JSObject* jsObject = toJS(object);
    JSValue* jsValue = toJS(value);

    jsObject->setPrototype(jsValue);
}

bool JSObjectHasProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName)
{
    JSLock lock;
    ExecState* exec = toJS(ctx);
    JSObject* jsObject = toJS(object);
    UString::Rep* nameRep = toJS(propertyName);
    
    return jsObject->hasProperty(exec, Identifier(exec, nameRep));
}

JSValueRef JSObjectGetProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    JSLock lock;
    ExecState* exec = toJS(ctx);
    JSObject* jsObject = toJS(object);
    UString::Rep* nameRep = toJS(propertyName);

    JSValue* jsValue = jsObject->get(exec, Identifier(exec, nameRep));
    if (exec->hadException()) {
        if (exception)
            *exception = toRef(exec->exception());
        exec->clearException();
    }
    return toRef(jsValue);
}

void JSObjectSetProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSPropertyAttributes attributes, JSValueRef* exception)
{
    JSLock lock;
    ExecState* exec = toJS(ctx);
    JSObject* jsObject = toJS(object);
    Identifier name(exec, toJS(propertyName));
    JSValue* jsValue = toJS(value);

    if (attributes && !jsObject->hasProperty(exec, name))
        jsObject->putWithAttributes(exec, name, jsValue, attributes);
    else
        jsObject->put(exec, name, jsValue);

    if (exec->hadException()) {
        if (exception)
            *exception = toRef(exec->exception());
        exec->clearException();
    }
}

JSValueRef JSObjectGetPropertyAtIndex(JSContextRef ctx, JSObjectRef object, unsigned propertyIndex, JSValueRef* exception)
{
    JSLock lock;
    ExecState* exec = toJS(ctx);
    JSObject* jsObject = toJS(object);

    JSValue* jsValue = jsObject->get(exec, propertyIndex);
    if (exec->hadException()) {
        if (exception)
            *exception = toRef(exec->exception());
        exec->clearException();
    }
    return toRef(jsValue);
}


void JSObjectSetPropertyAtIndex(JSContextRef ctx, JSObjectRef object, unsigned propertyIndex, JSValueRef value, JSValueRef* exception)
{
    JSLock lock;
    ExecState* exec = toJS(ctx);
    JSObject* jsObject = toJS(object);
    JSValue* jsValue = toJS(value);
    
    jsObject->put(exec, propertyIndex, jsValue);
    if (exec->hadException()) {
        if (exception)
            *exception = toRef(exec->exception());
        exec->clearException();
    }
}

bool JSObjectDeleteProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    JSLock lock;
    ExecState* exec = toJS(ctx);
    JSObject* jsObject = toJS(object);
    UString::Rep* nameRep = toJS(propertyName);

    bool result = jsObject->deleteProperty(exec, Identifier(exec, nameRep));
    if (exec->hadException()) {
        if (exception)
            *exception = toRef(exec->exception());
        exec->clearException();
    }
    return result;
}

void* JSObjectGetPrivate(JSObjectRef object)
{
    JSObject* jsObject = toJS(object);
    
    if (jsObject->inherits(&JSCallbackObject<JSGlobalObject>::info))
        return static_cast<JSCallbackObject<JSGlobalObject>*>(jsObject)->getPrivate();
    else if (jsObject->inherits(&JSCallbackObject<JSObject>::info))
        return static_cast<JSCallbackObject<JSObject>*>(jsObject)->getPrivate();
    
    return 0;
}

bool JSObjectSetPrivate(JSObjectRef object, void* data)
{
    JSObject* jsObject = toJS(object);
    
    if (jsObject->inherits(&JSCallbackObject<JSGlobalObject>::info)) {
        static_cast<JSCallbackObject<JSGlobalObject>*>(jsObject)->setPrivate(data);
        return true;
    } else if (jsObject->inherits(&JSCallbackObject<JSObject>::info)) {
        static_cast<JSCallbackObject<JSObject>*>(jsObject)->setPrivate(data);
        return true;
    }
        
    return false;
}

bool JSObjectIsFunction(JSContextRef, JSObjectRef object)
{
    CallData callData;
    return toJS(object)->getCallData(callData) != CallTypeNone;
}

JSValueRef JSObjectCallAsFunction(JSContextRef ctx, JSObjectRef object, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSLock lock;
    ExecState* exec = toJS(ctx);
    JSObject* jsObject = toJS(object);
    JSObject* jsThisObject = toJS(thisObject);

    if (!jsThisObject)
        jsThisObject = exec->globalThisValue();

    ArgList argList;
    for (size_t i = 0; i < argumentCount; i++)
        argList.append(toJS(arguments[i]));

    CallData callData;
    CallType callType = jsObject->getCallData(callData);
    if (callType == CallTypeNone)
        return 0;

    JSValueRef result = toRef(call(exec, jsObject, callType, callData, jsThisObject, argList));
    if (exec->hadException()) {
        if (exception)
            *exception = toRef(exec->exception());
        exec->clearException();
        result = 0;
    }
    return result;
}

bool JSObjectIsConstructor(JSContextRef, JSObjectRef object)
{
    JSObject* jsObject = toJS(object);
    ConstructData constructData;
    return jsObject->getConstructData(constructData) != ConstructTypeNone;
}

JSObjectRef JSObjectCallAsConstructor(JSContextRef ctx, JSObjectRef object, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSLock lock;
    ExecState* exec = toJS(ctx);
    JSObject* jsObject = toJS(object);

    ConstructData constructData;
    ConstructType constructType = jsObject->getConstructData(constructData);
    if (constructType == ConstructTypeNone)
        return 0;

    ArgList argList;
    for (size_t i = 0; i < argumentCount; i++)
        argList.append(toJS(arguments[i]));
    JSObjectRef result = toRef(construct(exec, jsObject, constructType, constructData, argList));
    if (exec->hadException()) {
        if (exception)
            *exception = toRef(exec->exception());
        exec->clearException();
        result = 0;
    }
    return result;
}
#include <wtf/FastAllocBase.h>

struct OpaqueJSPropertyNameArray: public WTF::FastAllocBase
{
    OpaqueJSPropertyNameArray(JSGlobalData* globalData) : refCount(0), array(globalData)
    {
    }
    
    unsigned refCount;
    PropertyNameArray array;
};

JSPropertyNameArrayRef JSObjectCopyPropertyNames(JSContextRef ctx, JSObjectRef object)
{
    JSLock lock;
    JSObject* jsObject = toJS(object);
    ExecState* exec = toJS(ctx);
    
    JSPropertyNameArrayRef propertyNames = new OpaqueJSPropertyNameArray(&exec->globalData());
    jsObject->getPropertyNames(exec, propertyNames->array);
    
    return JSPropertyNameArrayRetain(propertyNames);
}

JSPropertyNameArrayRef JSPropertyNameArrayRetain(JSPropertyNameArrayRef array)
{
    JSLock lock;
    ++array->refCount;
    return array;
}

void JSPropertyNameArrayRelease(JSPropertyNameArrayRef array)
{
    JSLock lock;
    if (--array->refCount == 0)
        delete array;
}

size_t JSPropertyNameArrayGetCount(JSPropertyNameArrayRef array)
{
    return array->array.size();
}

JSStringRef JSPropertyNameArrayGetNameAtIndex(JSPropertyNameArrayRef array, size_t index)
{
    return toRef(array->array[static_cast<unsigned>(index)].ustring().rep());
}

void JSPropertyNameAccumulatorAddName(JSPropertyNameAccumulatorRef array, JSStringRef propertyName)
{
    JSLock lock;
    PropertyNameArray* propertyNames = toJS(array);
    UString::Rep* rep = toJS(propertyName);
    propertyNames->add(rep);
}
