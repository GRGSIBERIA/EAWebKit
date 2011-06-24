/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2007, 2008 Apple Inc. All Rights Reserved.
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

/*
* This file was modified by Electronic Arts Inc Copyright � 2009
*/

#include "config.h"
#include "RegExpConstructor.h"
#include "RegExpConstructor.lut.h"

#include "ArrayPrototype.h"
#include "JSArray.h"
#include "JSString.h"
#include "ObjectPrototype.h"
#include "RegExpObject.h"
#include "RegExpPrototype.h"
#include "regexp.h"

namespace KJS {

const ClassInfo RegExpConstructor::info = { "Function", &InternalFunction::info, 0, ExecState::regExpConstructorTable };

/* Source for RegExpConstructor.lut.h
@begin regExpConstructorTable
  input           RegExpConstructor::Input          None
  $_              RegExpConstructor::Input          DontEnum
  multiline       RegExpConstructor::Multiline      None
  $*              RegExpConstructor::Multiline      DontEnum
  lastMatch       RegExpConstructor::LastMatch      DontDelete|ReadOnly
  $&              RegExpConstructor::LastMatch      DontDelete|ReadOnly|DontEnum
  lastParen       RegExpConstructor::LastParen      DontDelete|ReadOnly
  $+              RegExpConstructor::LastParen      DontDelete|ReadOnly|DontEnum
  leftContext     RegExpConstructor::LeftContext    DontDelete|ReadOnly
  $`              RegExpConstructor::LeftContext    DontDelete|ReadOnly|DontEnum
  rightContext    RegExpConstructor::RightContext   DontDelete|ReadOnly
  $'              RegExpConstructor::RightContext   DontDelete|ReadOnly|DontEnum
  $1              RegExpConstructor::Dollar1        DontDelete|ReadOnly
  $2              RegExpConstructor::Dollar2        DontDelete|ReadOnly
  $3              RegExpConstructor::Dollar3        DontDelete|ReadOnly
  $4              RegExpConstructor::Dollar4        DontDelete|ReadOnly
  $5              RegExpConstructor::Dollar5        DontDelete|ReadOnly
  $6              RegExpConstructor::Dollar6        DontDelete|ReadOnly
  $7              RegExpConstructor::Dollar7        DontDelete|ReadOnly
  $8              RegExpConstructor::Dollar8        DontDelete|ReadOnly
  $9              RegExpConstructor::Dollar9        DontDelete|ReadOnly
@end
*/
#include <wtf/FastAllocBase.h>

struct RegExpConstructorPrivate {
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
  // Global search cache / settings
  RegExpConstructorPrivate() : lastNumSubPatterns(0), multiline(false) { }
  UString lastInput;
  OwnArrayPtr<int> lastOvector;
  unsigned lastNumSubPatterns : 31;
  bool multiline              : 1;
};

RegExpConstructor::RegExpConstructor(ExecState* exec, FunctionPrototype* funcProto, RegExpPrototype* regProto)
  : InternalFunction(funcProto, Identifier(exec, "RegExp"))
  , d(new RegExpConstructorPrivate)
{
  // ECMA 15.10.5.1 RegExp.prototype
  putDirect(exec->propertyNames().prototype, regProto, DontEnum | DontDelete | ReadOnly);

  // no. of arguments for constructor
  putDirect(exec->propertyNames().length, jsNumber(exec, 2), ReadOnly | DontDelete | DontEnum);
}

/* 
  To facilitate result caching, exec(), test(), match(), search(), and replace() dipatch regular
  expression matching through the performMatch function. We use cached results to calculate, 
  e.g., RegExp.lastMatch and RegExp.leftParen.
*/
void RegExpConstructor::performMatch(RegExp* r, const UString& s, int startOffset, int& position, int& length, int** ovector)
{
  OwnArrayPtr<int> tmpOvector;
  position = r->match(s, startOffset, &tmpOvector);

  if (ovector)
    *ovector = tmpOvector.get();
  
  if (position != -1) {
    ASSERT(tmpOvector);

    length = tmpOvector[1] - tmpOvector[0];

    d->lastInput = s;
    d->lastOvector.set(tmpOvector.release());
    d->lastNumSubPatterns = r->numSubpatterns();
  }
}

class RegExpMatchesArray : public JSArray {
public:
    RegExpMatchesArray(ExecState*, RegExpConstructorPrivate*);
    virtual ~RegExpMatchesArray();

private:
    virtual bool getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot) { if (lazyCreationData()) fillArrayInstance(exec); return JSArray::getOwnPropertySlot(exec, propertyName, slot); }
    virtual bool getOwnPropertySlot(ExecState* exec, unsigned propertyName, PropertySlot& slot) { if (lazyCreationData()) fillArrayInstance(exec); return JSArray::getOwnPropertySlot(exec, propertyName, slot); }
    virtual void put(ExecState* exec, const Identifier& propertyName, JSValue* v) { if (lazyCreationData()) fillArrayInstance(exec); JSArray::put(exec, propertyName, v); }
    virtual void put(ExecState* exec, unsigned propertyName, JSValue* v) { if (lazyCreationData()) fillArrayInstance(exec); JSArray::put(exec, propertyName, v); }
    virtual bool deleteProperty(ExecState* exec, const Identifier& propertyName) { if (lazyCreationData()) fillArrayInstance(exec); return JSArray::deleteProperty(exec, propertyName); }
    virtual bool deleteProperty(ExecState* exec, unsigned propertyName) { if (lazyCreationData()) fillArrayInstance(exec); return JSArray::deleteProperty(exec, propertyName); }
    virtual void getPropertyNames(ExecState* exec, PropertyNameArray& arr) { if (lazyCreationData()) fillArrayInstance(exec); JSArray::getPropertyNames(exec, arr); }

    void fillArrayInstance(ExecState*);
};

RegExpMatchesArray::RegExpMatchesArray(ExecState* exec, RegExpConstructorPrivate* data)
    : JSArray(exec->lexicalGlobalObject()->arrayPrototype(), data->lastNumSubPatterns + 1)
{
    RegExpConstructorPrivate* d = new RegExpConstructorPrivate;
    d->lastInput = data->lastInput;
    d->lastNumSubPatterns = data->lastNumSubPatterns;
    unsigned offsetVectorSize = (data->lastNumSubPatterns + 1) * 2; // only copying the result part of the vector
    d->lastOvector.set(WTF::fastNewArray<int> (offsetVectorSize));
    memcpy(d->lastOvector.get(), data->lastOvector.get(), offsetVectorSize * sizeof(int));
    // d->multiline is not needed, and remains uninitialized

    setLazyCreationData(d);
}

RegExpMatchesArray::~RegExpMatchesArray()
{
    delete static_cast<RegExpConstructorPrivate*>(lazyCreationData());
}

void RegExpMatchesArray::fillArrayInstance(ExecState* exec)
{
    RegExpConstructorPrivate* d = static_cast<RegExpConstructorPrivate*>(lazyCreationData());
    ASSERT(d);

    unsigned lastNumSubpatterns = d->lastNumSubPatterns;

    for (unsigned i = 0; i <= lastNumSubpatterns; ++i) {
        int start = d->lastOvector[2 * i];
        if (start >= 0)
            JSArray::put(exec, i, jsString(exec, d->lastInput.substr(start, d->lastOvector[2 * i + 1] - start)));
    }
    JSArray::put(exec, exec->propertyNames().index, jsNumber(exec, d->lastOvector[0]));
    JSArray::put(exec, exec->propertyNames().input, jsString(exec, d->lastInput));

    delete d;
    setLazyCreationData(0);
}

JSObject* RegExpConstructor::arrayOfMatches(ExecState* exec) const
{
    return new (exec) RegExpMatchesArray(exec, d.get());
}

JSValue* RegExpConstructor::getBackref(ExecState* exec, unsigned i) const
{
  if (d->lastOvector && i <= d->lastNumSubPatterns)
    return jsString(exec, d->lastInput.substr(d->lastOvector[2 * i], d->lastOvector[2 * i + 1] - d->lastOvector[2 * i]));
  return jsString(exec, "");
}

JSValue* RegExpConstructor::getLastParen(ExecState* exec) const
{
  unsigned i = d->lastNumSubPatterns;
  if (i > 0) {
    ASSERT(d->lastOvector);
    return jsString(exec, d->lastInput.substr(d->lastOvector[2 * i], d->lastOvector[2 * i + 1] - d->lastOvector[2 * i]));
  }
  return jsString(exec, "");
}

JSValue* RegExpConstructor::getLeftContext(ExecState* exec) const
{
  if (d->lastOvector)
    return jsString(exec, d->lastInput.substr(0, d->lastOvector[0]));
  return jsString(exec, "");
}

JSValue* RegExpConstructor::getRightContext(ExecState* exec) const
{
  if (d->lastOvector) {
    UString s = d->lastInput;
    return jsString(exec, s.substr(d->lastOvector[1], s.size() - d->lastOvector[1]));
  }
  return jsString(exec, "");
}

bool RegExpConstructor::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<RegExpConstructor, InternalFunction>(exec, ExecState::regExpConstructorTable(exec), this, propertyName, slot);
}

JSValue *RegExpConstructor::getValueProperty(ExecState* exec, int token) const
{
  switch (token) {
    case Dollar1:
      return getBackref(exec, 1);
    case Dollar2:
      return getBackref(exec, 2);
    case Dollar3:
      return getBackref(exec, 3);
    case Dollar4:
      return getBackref(exec, 4);
    case Dollar5:
      return getBackref(exec, 5);
    case Dollar6:
      return getBackref(exec, 6);
    case Dollar7:
      return getBackref(exec, 7);
    case Dollar8:
      return getBackref(exec, 8);
    case Dollar9:
      return getBackref(exec, 9);
    case Input:
      return jsString(exec, d->lastInput);
    case Multiline:
      return jsBoolean(d->multiline);
    case LastMatch:
      return getBackref(exec, 0);
    case LastParen:
      return getLastParen(exec);
    case LeftContext:
      return getLeftContext(exec);
    case RightContext:
      return getRightContext(exec);
    default:
      ASSERT_NOT_REACHED();
  }

  return jsString(exec, "");
}

void RegExpConstructor::put(ExecState *exec, const Identifier &propertyName, JSValue *value)
{
    lookupPut<RegExpConstructor, InternalFunction>(exec, propertyName, value, ExecState::regExpConstructorTable(exec), this);
}

void RegExpConstructor::putValueProperty(ExecState *exec, int token, JSValue *value)
{
  switch (token) {
    case Input:
      d->lastInput = value->toString(exec);
      break;
    case Multiline:
      d->multiline = value->toBoolean(exec);
      break;
    default:
      ASSERT(0);
  }
}
  
// ECMA 15.10.4
static JSObject* constructRegExp(ExecState* exec, const ArgList& args)
{
  JSValue* arg0 = args[0];
  JSValue* arg1 = args[1];
  
  if (arg0->isObject(&RegExpObject::info)) {
    if (!arg1->isUndefined())
      return throwError(exec, TypeError, "Cannot supply flags when constructing one RegExp from another.");
    return static_cast<JSObject*>(arg0);
  }
  
  UString pattern = arg0->isUndefined() ? UString("") : arg0->toString(exec);
  UString flags = arg1->isUndefined() ? UString("") : arg1->toString(exec);
  
  RefPtr<RegExp> regExp = RegExp::create(pattern, flags);
  return regExp->isValid()
    ? new (exec) RegExpObject(exec->lexicalGlobalObject()->regExpPrototype(), regExp.release())
    : throwError(exec, SyntaxError, UString("Invalid regular expression: ").append(regExp->errorMessage()));
}

static JSObject* constructWithRegExpConstructor(ExecState* exec, JSObject*, const ArgList& args)
{
    return constructRegExp(exec, args);
}

ConstructType RegExpConstructor::getConstructData(ConstructData& constructData)
{
    constructData.native.function = constructWithRegExpConstructor;
    return ConstructTypeNative;
}

// ECMA 15.10.3
static JSValue* callRegExpConstructor(ExecState* exec, JSObject*, JSValue*, const ArgList& args)
{
    return constructRegExp(exec, args);
}

CallType RegExpConstructor::getCallData(CallData& callData)
{
    callData.native.function = callRegExpConstructor;
    return CallTypeNative;
}

const UString& RegExpConstructor::input() const
{
    // Can detect a distinct initial state that is invisible to JavaScript, by checking for null
    // state (since jsString turns null strings to empty strings).
    return d->lastInput;
}

} // namespace KJS
