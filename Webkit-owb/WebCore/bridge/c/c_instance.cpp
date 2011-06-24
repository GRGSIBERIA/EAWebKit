/*
 * Copyright (C) 2003, 2006 Apple Computer, Inc.  All rights reserved.
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

#if ENABLE(NETSCAPE_PLUGIN_API)

#include "c_instance.h"

#include "c_class.h"
#include "c_runtime.h"
#include "c_utility.h"
#include "npruntime_impl.h"
#include "runtime_root.h"
#include <kjs/ExecState.h>
#include <kjs/PropertyNameArray.h>
#include <wtf/Assertions.h>
#include <wtf/StringExtras.h>
#include <wtf/Vector.h>

namespace KJS {
namespace Bindings {

CInstance::CInstance(NPObject* o, PassRefPtr<RootObject> rootObject)
    : Instance(rootObject)
{
    _object = _NPN_RetainObject(o);
    _class = 0;
}

CInstance::~CInstance() 
{
    _NPN_ReleaseObject(_object);
}

Class *CInstance::getClass() const
{
    if (!_class)
        _class = CClass::classForIsA(_object->_class);
    return _class;
}

bool CInstance::supportsInvokeDefaultMethod() const
{
    return _object->_class->invokeDefault;
}

JSValue* CInstance::invokeMethod(ExecState* exec, const MethodList& methodList, const ArgList& args)
{
    // Overloading methods are not allowed by NPObjects.  Should only be one
    // name match for a particular method.
    ASSERT(methodList.size() == 1);

    CMethod* method = static_cast<CMethod*>(methodList[0]);

    NPIdentifier ident = _NPN_GetStringIdentifier(method->name());
    if (!_object->_class->hasMethod(_object, ident))
        return jsUndefined();

    unsigned count = args.size();
    Vector<NPVariant, 128> cArgs(count);

    unsigned i;
    for (i = 0; i < count; i++)
        convertValueToNPVariant(exec, args.at(i), &cArgs[i]);

    // Invoke the 'C' method.
    NPVariant resultVariant;
    VOID_TO_NPVARIANT(resultVariant);

    {
       JSLock::DropAllLocks dropAllLocks;
        _object->_class->invoke(_object, ident, cArgs.data(), count, &resultVariant);
    }

    for (i = 0; i < count; i++)
        _NPN_ReleaseVariantValue(&cArgs[i]);

    JSValue* resultValue = convertNPVariantToValue(exec, &resultVariant, _rootObject.get());
    _NPN_ReleaseVariantValue(&resultVariant);
    return resultValue;
}


JSValue* CInstance::invokeDefaultMethod(ExecState* exec, const ArgList& args)
{
    if (!_object->_class->invokeDefault)
        return jsUndefined();

    unsigned count = args.size();
    Vector<NPVariant, 128> cArgs(count);

    unsigned i;
    for (i = 0; i < count; i++)
        convertValueToNPVariant(exec, args.at(i), &cArgs[i]);

    // Invoke the 'C' method.
    NPVariant resultVariant;
    VOID_TO_NPVARIANT(resultVariant);
    {
       JSLock::DropAllLocks dropAllLocks;
        _object->_class->invokeDefault(_object, cArgs.data(), count, &resultVariant);
    }
    
    for (i = 0; i < count; i++)
        _NPN_ReleaseVariantValue(&cArgs[i]);

    JSValue* resultValue = convertNPVariantToValue(exec, &resultVariant, _rootObject.get());
    _NPN_ReleaseVariantValue(&resultVariant);
    return resultValue;
}


JSValue* CInstance::defaultValue(ExecState* exec, JSType hint) const
{
    if (hint == StringType)
        return stringValue(exec);
    if (hint == NumberType)
        return numberValue(exec);
   if (hint == BooleanType)
        return booleanValue();
    return valueOf(exec);
}

JSValue* CInstance::stringValue(ExecState* exec) const
{
    char buf[1024];
    snprintf(buf, sizeof(buf), "NPObject %p, NPClass %p", _object, _object->_class);
    return jsString(exec, buf);
}

JSValue* CInstance::numberValue(ExecState* exec) const
{
    // FIXME: Implement something sensible.
    return jsNumber(exec, 0);
}

JSValue* CInstance::booleanValue() const
{
    // FIXME: Implement something sensible.
    return jsBoolean(false);
}

JSValue* CInstance::valueOf(ExecState* exec) const 
{
    return stringValue(exec);
}

void CInstance::getPropertyNames(ExecState* exec, PropertyNameArray& nameArray) 
{
    if (!NP_CLASS_STRUCT_VERSION_HAS_ENUM(_object->_class) ||
        !_object->_class->enumerate)
        return;

    unsigned count;
    NPIdentifier* identifiers;
    
    {
        JSLock::DropAllLocks dropAllLocks;
        if (!_object->_class->enumerate(_object, &identifiers, &count))
            return;
    }
    
    for (unsigned i = 0; i < count; i++) {
        PrivateIdentifier* identifier = static_cast<PrivateIdentifier*>(identifiers[i]);
        
        if (identifier->isString)
            nameArray.add(identifierFromNPIdentifier(identifier->value.string));
        else
            nameArray.add(Identifier::from(exec, identifier->value.number));
    }
         
    // FIXME: This should really call NPN_MemFree but that's in WebKit
    WTF::fastFree(identifiers);
}

}
}

#endif // ENABLE(NETSCAPE_PLUGIN_API)
