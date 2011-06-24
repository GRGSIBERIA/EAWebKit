// -*- c-basic-offset: 2 -*-
/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Cameron Zwarich (cwzwarich@uwaterloo.ca)
 *  Copyright (C) 2007 Maks Orlovich
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

/*
* This file was modified by Electronic Arts Inc Copyright � 2009
*/

#ifndef JSFunction_h
#define JSFunction_h

#include <wtf/FastAllocBase.h>
#include "JSVariableObject.h"
#include "SymbolTable.h"
#include "nodes.h"
#include "JSObject.h"

namespace KJS {

  class FunctionBodyNode;
  class FunctionPrototype;
  class JSActivation;
  class JSGlobalObject;

  class InternalFunction : public JSObject {
  public:
    static const ClassInfo info;
    virtual const ClassInfo* classInfo() const { return &info; }
    const Identifier& functionName() const { return m_name; }

  protected:
    InternalFunction();
    InternalFunction(FunctionPrototype*, const Identifier&);

  private:
    virtual CallType getCallData(CallData&) = 0;
    virtual bool implementsHasInstance() const;

    Identifier m_name;
  };

  class JSFunction : public InternalFunction {
  public:
    JSFunction(ExecState*, const Identifier&, FunctionBodyNode*, ScopeChainNode*);

    virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
    virtual void put(ExecState*, const Identifier& propertyName, JSValue*);
    virtual bool deleteProperty(ExecState*, const Identifier& propertyName);

    JSObject* construct(ExecState*, const ArgList&);
    JSValue* call(ExecState*, JSValue* thisValue, const ArgList&);

    // Note: Returns a null identifier for any parameters that will never get set
    // due to a later parameter with the same name.
    const Identifier& getParameterName(int index);

    static const ClassInfo info;

    RefPtr<FunctionBodyNode> body;

    void setScope(const ScopeChain& s) { _scope = s; }
    ScopeChain& scope() { return _scope; }

    virtual void mark();

  private:
    virtual const ClassInfo* classInfo() const { return &info; }
    virtual ConstructType getConstructData(ConstructData&);
    virtual CallType getCallData(CallData&);

    ScopeChain _scope;

    static JSValue* argumentsGetter(ExecState*, const Identifier&, const PropertySlot&);
    static JSValue* callerGetter(ExecState*, const Identifier&, const PropertySlot&);
    static JSValue* lengthGetter(ExecState*, const Identifier&, const PropertySlot&);
  };

  class IndexToNameMap {
public:
// Placement operator new.
void* operator new(size_t, void* p) { return p; }
void* operator new[](size_t, void* p) { return p; }
 
void* operator new(size_t size)
{
     void* p = fastMalloc(size);
     fastMallocMatchValidateMalloc(p, WTF::Internal::AllocTypeClassNew);
     return p;
}
 
void operator delete(void* p)
{
     fastMallocMatchValidateFree(p, WTF::Internal::AllocTypeClassNew);
     fastFree(p);  // We don't need to check for a null pointer; the compiler does this.
}
 
void* operator new[](size_t size)
{
     void* p = fastMalloc(size);
     fastMallocMatchValidateMalloc(p, WTF::Internal::AllocTypeClassNewArray);
     return p;
}
 
void operator delete[](void* p)
{
     fastMallocMatchValidateFree(p, WTF::Internal::AllocTypeClassNewArray);
     fastFree(p);  // We don't need to check for a null pointer; the compiler does this.
}
  public:
    IndexToNameMap(JSFunction*, const ArgList&);
    ~IndexToNameMap();
    
    Identifier& operator[](const Identifier& index);
    bool isMapped(const Identifier& index) const;
    void unMap(ExecState* exec, const Identifier& index);
    
  private:
    unsigned size;
    Identifier* _map;
  };
  
  class Arguments : public JSObject {
  public:
    Arguments(ExecState*, JSFunction* func, const ArgList& args, JSActivation* act);
    virtual void mark();
    virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
    virtual void put(ExecState*, const Identifier& propertyName, JSValue*);
    virtual bool deleteProperty(ExecState*, const Identifier& propertyName);
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
  private:
    static JSValue* mappedIndexGetter(ExecState*, const Identifier&, const PropertySlot& slot);

    JSActivation* _activationObject;
    mutable IndexToNameMap indexToNameMap;
  };

  class PrototypeFunction : public InternalFunction {
  public:
    PrototypeFunction(ExecState*, int len, const Identifier&, NativeFunction);
    PrototypeFunction(ExecState*, FunctionPrototype*, int len, const Identifier&, NativeFunction);

  private:
    virtual CallType getCallData(CallData&);

    const NativeFunction m_function;
  };

    class GlobalEvalFunction : public PrototypeFunction {
    public:
        GlobalEvalFunction(ExecState*, FunctionPrototype*, int len, const Identifier&, NativeFunction, JSGlobalObject* expectedThisObject);
        JSGlobalObject* cachedGlobalObject() const { return m_cachedGlobalObject; }

    private:
        virtual void mark();

        JSGlobalObject* m_cachedGlobalObject;
    };

    // Global Functions
    JSValue* globalFuncEval(ExecState*, JSObject*, JSValue*, const ArgList&);
    JSValue* globalFuncParseInt(ExecState*, JSObject*, JSValue*, const ArgList&);
    JSValue* globalFuncParseFloat(ExecState*, JSObject*, JSValue*, const ArgList&);
    JSValue* globalFuncIsNaN(ExecState*, JSObject*, JSValue*, const ArgList&);
    JSValue* globalFuncIsFinite(ExecState*, JSObject*, JSValue*, const ArgList&);
    JSValue* globalFuncDecodeURI(ExecState*, JSObject*, JSValue*, const ArgList&);
    JSValue* globalFuncDecodeURIComponent(ExecState*, JSObject*, JSValue*, const ArgList&);
    JSValue* globalFuncEncodeURI(ExecState*, JSObject*, JSValue*, const ArgList&);
    JSValue* globalFuncEncodeURIComponent(ExecState*, JSObject*, JSValue*, const ArgList&);
    JSValue* globalFuncEscape(ExecState*, JSObject*, JSValue*, const ArgList&);
    JSValue* globalFuncUnescape(ExecState*, JSObject*, JSValue*, const ArgList&);
#ifndef NDEBUG
    JSValue* globalFuncKJSPrint(ExecState*, JSObject*, JSValue*, const ArgList&);
#endif

    static const double mantissaOverflowLowerBound = 9007199254740992.0;
    double parseIntOverflow(const char*, int length, int radix);

} // namespace

#endif
