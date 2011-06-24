/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2008 Apple Inc. All rights reserved.
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
 *
 */

#include "config.h"
#include "ErrorPrototype.h"

#include "FunctionPrototype.h"
#include "ObjectPrototype.h"
#include "ustring.h"

namespace KJS {

static JSValue* errorProtoFuncToString(ExecState*, JSObject*, JSValue*, const ArgList&);

// ECMA 15.9.4
ErrorPrototype::ErrorPrototype(ExecState* exec, ObjectPrototype* objectPrototype, FunctionPrototype* functionPrototype)
    : ErrorInstance(objectPrototype)
{
    // The constructor will be added later in ErrorConstructor's constructor

    putDirect(exec->propertyNames().name, jsString(exec, "Error"), DontEnum);
    putDirect(exec->propertyNames().message, jsString(exec, "Unknown error"), DontEnum);

    putDirectFunction(new (exec) PrototypeFunction(exec, functionPrototype, 0, exec->propertyNames().toString, errorProtoFuncToString), DontEnum);
}

JSValue* errorProtoFuncToString(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    JSObject* thisObj = thisValue->toThisObject(exec);

    UString s = "Error";

    JSValue* v = thisObj->get(exec, exec->propertyNames().name);
    if (!v->isUndefined())
        s = v->toString(exec);

    v = thisObj->get(exec, exec->propertyNames().message);
    if (!v->isUndefined()) {
        // Mozilla-compatible format.
        s += ": ";
        s += v->toString(exec);
    }

    return jsString(exec, s);
}

} // namespace KJS
