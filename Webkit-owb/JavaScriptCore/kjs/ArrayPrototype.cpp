/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2007, 2008 Apple Inc. All rights reserved.
 *  Copyright (C) 2003 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 *
 */

#include "config.h"
#include "ArrayPrototype.h"

#include "Machine.h"
#include "ObjectPrototype.h"
#include "lookup.h"
#include "operations.h"
#include <wtf/Assertions.h>
#include <wtf/HashSet.h>
#include <algorithm> // for std::min

namespace KJS {

static JSValue* arrayProtoFuncToString(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* arrayProtoFuncToLocaleString(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* arrayProtoFuncConcat(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* arrayProtoFuncJoin(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* arrayProtoFuncPop(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* arrayProtoFuncPush(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* arrayProtoFuncReverse(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* arrayProtoFuncShift(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* arrayProtoFuncSlice(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* arrayProtoFuncSort(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* arrayProtoFuncSplice(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* arrayProtoFuncUnShift(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* arrayProtoFuncEvery(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* arrayProtoFuncForEach(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* arrayProtoFuncSome(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* arrayProtoFuncIndexOf(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* arrayProtoFuncFilter(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* arrayProtoFuncMap(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* arrayProtoFuncLastIndexOf(ExecState*, JSObject*, JSValue*, const ArgList&);

}

#include "ArrayPrototype.lut.h"

namespace KJS {

// ------------------------------ ArrayPrototype ----------------------------

const ClassInfo ArrayPrototype::info = {"Array", &JSArray::info, 0, ExecState::arrayTable};

/* Source for ArrayPrototype.lut.h
@begin arrayTable 16
  toString       arrayProtoFuncToString       DontEnum|Function 0
  toLocaleString arrayProtoFuncToLocaleString DontEnum|Function 0
  concat         arrayProtoFuncConcat         DontEnum|Function 1
  join           arrayProtoFuncJoin           DontEnum|Function 1
  pop            arrayProtoFuncPop            DontEnum|Function 0
  push           arrayProtoFuncPush           DontEnum|Function 1
  reverse        arrayProtoFuncReverse        DontEnum|Function 0
  shift          arrayProtoFuncShift          DontEnum|Function 0
  slice          arrayProtoFuncSlice          DontEnum|Function 2
  sort           arrayProtoFuncSort           DontEnum|Function 1
  splice         arrayProtoFuncSplice         DontEnum|Function 2
  unshift        arrayProtoFuncUnShift        DontEnum|Function 1
  every          arrayProtoFuncEvery          DontEnum|Function 1
  forEach        arrayProtoFuncForEach        DontEnum|Function 1
  some           arrayProtoFuncSome           DontEnum|Function 1
  indexOf        arrayProtoFuncIndexOf        DontEnum|Function 1
  lastIndexOf    arrayProtoFuncLastIndexOf    DontEnum|Function 1
  filter         arrayProtoFuncFilter         DontEnum|Function 1
  map            arrayProtoFuncMap            DontEnum|Function 1
@end
*/

// ECMA 15.4.4
ArrayPrototype::ArrayPrototype(ExecState*, ObjectPrototype* objProto)
    : JSArray(objProto, 0)
{
}

bool ArrayPrototype::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticFunctionSlot<JSArray>(exec, ExecState::arrayTable(exec), this, propertyName, slot);
}


// ------------------------------ Array Functions ----------------------------

// Helper function
static JSValue* getProperty(ExecState* exec, JSObject* obj, unsigned index)
{
    PropertySlot slot(obj);
    if (!obj->getPropertySlot(exec, index, slot))
        return 0;
    return slot.getValue(exec, index);
}

JSValue* arrayProtoFuncToString(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&JSArray::info))
        return throwError(exec, TypeError);
    JSObject* thisObj = static_cast<JSArray*>(thisValue);

    HashSet<JSObject*>& arrayVisitedElements = exec->dynamicGlobalObject()->arrayVisitedElements();
    if (arrayVisitedElements.size() > MaxReentryDepth)
        return throwError(exec, RangeError, "Maximum call stack size exceeded.");

    bool alreadyVisited = !arrayVisitedElements.add(thisObj).second;
    Vector<UChar, 256> strBuffer;
    if (alreadyVisited)
        return jsString(exec, UString(0, 0)); // return an empty string, avoding infinite recursion.

    unsigned length = thisObj->get(exec, exec->propertyNames().length)->toUInt32(exec);
    for (unsigned k = 0; k < length; k++) {
        if (k >= 1)
            strBuffer.append(',');
        if (!strBuffer.data()) {
            JSObject* error = Error::create(exec, GeneralError, "Out of memory");
            exec->setException(error);
            break;
        }

        JSValue* element = thisObj->get(exec, k);
        if (element->isUndefinedOrNull())
            continue;

        UString str = element->toString(exec);
        strBuffer.append(str.data(), str.size());

        if (!strBuffer.data()) {
            JSObject* error = Error::create(exec, GeneralError, "Out of memory");
            exec->setException(error);
        }

        if (exec->hadException())
            break;
    }
    exec->dynamicGlobalObject()->arrayVisitedElements().remove(thisObj);
    return jsString(exec, UString(strBuffer.data(), strBuffer.data() ? strBuffer.size() : 0));
}

JSValue* arrayProtoFuncToLocaleString(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&JSArray::info))
        return throwError(exec, TypeError);
    JSObject* thisObj = static_cast<JSArray*>(thisValue);

    HashSet<JSObject*>& arrayVisitedElements = exec->dynamicGlobalObject()->arrayVisitedElements();
    if (arrayVisitedElements.size() > MaxReentryDepth)
        return throwError(exec, RangeError, "Maximum call stack size exceeded.");

    bool alreadyVisited = !arrayVisitedElements.add(thisObj).second;
    Vector<UChar, 256> strBuffer;
    if (alreadyVisited)
        return jsString(exec, UString(0, 0)); // return an empty string, avoding infinite recursion.

    unsigned length = thisObj->get(exec, exec->propertyNames().length)->toUInt32(exec);
    for (unsigned k = 0; k < length; k++) {
        if (k >= 1)
            strBuffer.append(',');
        if (!strBuffer.data()) {
            JSObject* error = Error::create(exec, GeneralError, "Out of memory");
            exec->setException(error);
            break;
        }

        JSValue* element = thisObj->get(exec, k);
        if (element->isUndefinedOrNull())
            continue;

        JSObject* o = element->toObject(exec);
        JSValue* conversionFunction = o->get(exec, exec->propertyNames().toLocaleString);
        UString str;
        CallData callData;
        CallType callType = conversionFunction->getCallData(callData);
        if (callType != CallTypeNone)
            str = call(exec, conversionFunction, callType, callData, element, exec->emptyList())->toString(exec);
        else
            str = element->toString(exec);
        strBuffer.append(str.data(), str.size());

        if (!strBuffer.data()) {
            JSObject* error = Error::create(exec, GeneralError, "Out of memory");
            exec->setException(error);
        }

        if (exec->hadException())
            break;
    }
    exec->dynamicGlobalObject()->arrayVisitedElements().remove(thisObj);
    return jsString(exec, UString(strBuffer.data(), strBuffer.data() ? strBuffer.size() : 0));
}

JSValue* arrayProtoFuncJoin(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    JSObject* thisObj = thisValue->toThisObject(exec);

    HashSet<JSObject*>& arrayVisitedElements = exec->dynamicGlobalObject()->arrayVisitedElements();
    if (arrayVisitedElements.size() > MaxReentryDepth)
        return throwError(exec, RangeError, "Maximum call stack size exceeded.");

    bool alreadyVisited = !arrayVisitedElements.add(thisObj).second;
    Vector<UChar, 256> strBuffer;
    if (alreadyVisited)
        return jsString(exec, UString(0, 0)); // return an empty string, avoding infinite recursion.

    UChar comma = ',';
    UString separator = args[0]->isUndefined() ? UString(&comma, 1) : args[0]->toString(exec);

    unsigned length = thisObj->get(exec, exec->propertyNames().length)->toUInt32(exec);
    for (unsigned k = 0; k < length; k++) {
        if (k >= 1)
            strBuffer.append(separator.data(), separator.size());
        if (!strBuffer.data()) {
            JSObject* error = Error::create(exec, GeneralError, "Out of memory");
            exec->setException(error);
            break;
        }

        JSValue* element = thisObj->get(exec, k);
        if (element->isUndefinedOrNull())
            continue;

        UString str = element->toString(exec);
        strBuffer.append(str.data(), str.size());

        if (!strBuffer.data()) {
            JSObject* error = Error::create(exec, GeneralError, "Out of memory");
            exec->setException(error);
        }

        if (exec->hadException())
            break;
    }
    exec->dynamicGlobalObject()->arrayVisitedElements().remove(thisObj);
    return jsString(exec, UString(strBuffer.data(), strBuffer.data() ? strBuffer.size() : 0));
}

JSValue* arrayProtoFuncConcat(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    JSArray* arr = constructEmptyArray(exec);
    int n = 0;
    JSValue* curArg = thisValue->toThisObject(exec);
    ArgList::const_iterator it = args.begin();
    ArgList::const_iterator end = args.end();
    while (1) {
        if (curArg->isObject(&JSArray::info)) {
            JSArray* curArray = static_cast<JSArray*>(curArg);
            unsigned length = curArray->getLength();
            for (unsigned k = 0; k < length; ++k) {
                if (JSValue* v = getProperty(exec, curArray, k))
                    arr->put(exec, n, v);
                n++;
            }
        } else {
            arr->put(exec, n, curArg);
            n++;
        }
        if (it == end)
            break;
        curArg = *it;
        ++it;
    }
    arr->setLength(n);
    return arr;
}

JSValue* arrayProtoFuncPop(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    JSObject* thisObj = thisValue->toThisObject(exec);
    JSValue* result = 0;
    unsigned length = thisObj->get(exec, exec->propertyNames().length)->toUInt32(exec);
    if (length == 0) {
        thisObj->put(exec, exec->propertyNames().length, jsNumber(exec, length));
        result = jsUndefined();
    } else {
        result = thisObj->get(exec, length - 1);
        thisObj->deleteProperty(exec, length - 1);
        thisObj->put(exec, exec->propertyNames().length, jsNumber(exec, length - 1));
    }
    return result;
}

JSValue* arrayProtoFuncPush(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    JSObject* thisObj = thisValue->toThisObject(exec);
    unsigned length = thisObj->get(exec, exec->propertyNames().length)->toUInt32(exec);
    for (unsigned n = 0; n < args.size(); n++)
        thisObj->put(exec, length + n, args[n]);
    length += args.size();
    thisObj->put(exec, exec->propertyNames().length, jsNumber(exec, length));
    return jsNumber(exec, length);
}

JSValue* arrayProtoFuncReverse(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    JSObject* thisObj = thisValue->toThisObject(exec);
    unsigned length = thisObj->get(exec, exec->propertyNames().length)->toUInt32(exec);
    unsigned middle = length / 2;

    for (unsigned k = 0; k < middle; k++) {
        unsigned lk1 = length - k - 1;
        JSValue* obj2 = getProperty(exec, thisObj, lk1);
        JSValue* obj = getProperty(exec, thisObj, k);

        if (obj2)
            thisObj->put(exec, k, obj2);
        else
            thisObj->deleteProperty(exec, k);

        if (obj)
            thisObj->put(exec, lk1, obj);
        else
            thisObj->deleteProperty(exec, lk1);
    }
    return thisObj;
}

JSValue* arrayProtoFuncShift(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    JSObject* thisObj = thisValue->toThisObject(exec);
    JSValue* result = 0;

    unsigned length = thisObj->get(exec, exec->propertyNames().length)->toUInt32(exec);
    if (length == 0) {
        thisObj->put(exec, exec->propertyNames().length, jsNumber(exec, length));
        result = jsUndefined();
    } else {
        result = thisObj->get(exec, 0);
        for (unsigned k = 1; k < length; k++) {
            if (JSValue* obj = getProperty(exec, thisObj, k))
                thisObj->put(exec, k - 1, obj);
            else
                thisObj->deleteProperty(exec, k - 1);
        }
        thisObj->deleteProperty(exec, length - 1);
        thisObj->put(exec, exec->propertyNames().length, jsNumber(exec, length - 1));
    }
    return result;
}

JSValue* arrayProtoFuncSlice(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    // http://developer.netscape.com/docs/manuals/js/client/jsref/array.htm#1193713 or 15.4.4.10

    JSObject* thisObj = thisValue->toThisObject(exec);

    // We return a new array
    JSArray* resObj = constructEmptyArray(exec);
    JSValue* result = resObj;
    double begin = args[0]->toInteger(exec);
    unsigned length = thisObj->get(exec, exec->propertyNames().length)->toUInt32(exec);
    if (begin >= 0) {
        if (begin > length)
            begin = length;
    } else {
        begin += length;
        if (begin < 0)
            begin = 0;
    }
    double end;
    if (args[1]->isUndefined())
        end = length;
    else {
        end = args[1]->toInteger(exec);
        if (end < 0) {
            end += length;
            if (end < 0)
                end = 0;
        } else {
            if (end > length)
                end = length;
        }
    }

    int n = 0;
    int b = static_cast<int>(begin);
    int e = static_cast<int>(end);
    for (int k = b; k < e; k++, n++) {
        if (JSValue* v = getProperty(exec, thisObj, k))
            resObj->put(exec, n, v);
    }
    resObj->setLength(n);
    return result;
}

JSValue* arrayProtoFuncSort(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    JSObject* thisObj = thisValue->toThisObject(exec);

    JSValue* function = args[0];
    CallData callData;
    CallType callType = function->getCallData(callData);

    if (thisObj->classInfo() == &JSArray::info) {
        if (callType != CallTypeNone)
            static_cast<JSArray*>(thisObj)->sort(exec, function, callType, callData);
        else
            static_cast<JSArray*>(thisObj)->sort(exec);
        return thisObj;
    }

    unsigned length = thisObj->get(exec, exec->propertyNames().length)->toUInt32(exec);

    if (!length)
        return thisObj;

    // "Min" sort. Not the fastest, but definitely less code than heapsort
    // or quicksort, and much less swapping than bubblesort/insertionsort.
    for (unsigned i = 0; i < length - 1; ++i) {
        JSValue* iObj = thisObj->get(exec, i);
        unsigned themin = i;
        JSValue* minObj = iObj;
        for (unsigned j = i + 1; j < length; ++j) {
            JSValue* jObj = thisObj->get(exec, j);
            double compareResult;
            if (jObj->isUndefined())
                compareResult = 1; // don't check minObj because there's no need to differentiate == (0) from > (1)
            else if (minObj->isUndefined())
                compareResult = -1;
            else if (callType != CallTypeNone) {
                ArgList l;
                l.append(jObj);
                l.append(minObj);
                compareResult = call(exec, function, callType, callData, exec->globalThisValue(), l)->toNumber(exec);
            } else
                compareResult = (jObj->toString(exec) < minObj->toString(exec)) ? -1 : 1;

            if (compareResult < 0) {
                themin = j;
                minObj = jObj;
            }
        }
        // Swap themin and i
        if (themin > i) {
            thisObj->put(exec, i, minObj);
            thisObj->put(exec, themin, iObj);
        }
    }
    return thisObj;
}

JSValue* arrayProtoFuncSplice(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    JSObject* thisObj = thisValue->toThisObject(exec);

    // 15.4.4.12
    JSArray* resObj = constructEmptyArray(exec);
    JSValue* result = resObj;
    unsigned length = thisObj->get(exec, exec->propertyNames().length)->toUInt32(exec);
    if (!args.size())
        return jsUndefined();
    int begin = args[0]->toUInt32(exec);
    if (begin < 0)
        begin = std::max<int>(begin + length, 0);
    else
        begin = std::min<int>(begin, length);

    unsigned deleteCount;
    if (args.size() > 1)
        deleteCount = std::min<int>(std::max<int>(args[1]->toUInt32(exec), 0), length - begin);
    else
        deleteCount = length - begin;

    for (unsigned k = 0; k < deleteCount; k++) {
        if (JSValue* v = getProperty(exec, thisObj, k + begin))
            resObj->put(exec, k, v);
    }
    resObj->setLength(deleteCount);

    unsigned additionalArgs = std::max<int>(args.size() - 2, 0);
    if (additionalArgs != deleteCount) {
        if (additionalArgs < deleteCount) {
            for (unsigned k = begin; k < length - deleteCount; ++k) {
                if (JSValue* v = getProperty(exec, thisObj, k + deleteCount))
                    thisObj->put(exec, k + additionalArgs, v);
                else
                    thisObj->deleteProperty(exec, k + additionalArgs);
            }
            for (unsigned k = length; k > length - deleteCount + additionalArgs; --k)
                thisObj->deleteProperty(exec, k - 1);
        } else {
            for (unsigned k = length - deleteCount; (int)k > begin; --k) {
                if (JSValue* obj = getProperty(exec, thisObj, k + deleteCount - 1))
                    thisObj->put(exec, k + additionalArgs - 1, obj);
                else
                    thisObj->deleteProperty(exec, k + additionalArgs - 1);
            }
        }
    }
    for (unsigned k = 0; k < additionalArgs; ++k)
        thisObj->put(exec, k + begin, args[k + 2]);

    thisObj->put(exec, exec->propertyNames().length, jsNumber(exec, length - deleteCount + additionalArgs));
    return result;
}

JSValue* arrayProtoFuncUnShift(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    JSObject* thisObj = thisValue->toThisObject(exec);

    // 15.4.4.13
    unsigned length = thisObj->get(exec, exec->propertyNames().length)->toUInt32(exec);
    unsigned nrArgs = args.size();
    if (nrArgs) {
        for (unsigned k = length; k > 0; --k) {
            if (JSValue* v = getProperty(exec, thisObj, k - 1))
                thisObj->put(exec, k + nrArgs - 1, v);
            else
                thisObj->deleteProperty(exec, k + nrArgs - 1);
        }
    }
    for (unsigned k = 0; k < nrArgs; ++k)
        thisObj->put(exec, k, args[k]);
    JSValue* result = jsNumber(exec, length + nrArgs);
    thisObj->put(exec, exec->propertyNames().length, result);
    return result;
}

JSValue* arrayProtoFuncFilter(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    JSObject* thisObj = thisValue->toThisObject(exec);

    JSValue* function = args[0];
    CallData callData;
    CallType callType = function->getCallData(callData);
    if (callType == CallTypeNone)
        return throwError(exec, TypeError);

    JSObject* applyThis = args[1]->isUndefinedOrNull() ? exec->globalThisValue() : args[1]->toObject(exec);
    JSArray* resultArray = constructEmptyArray(exec);

    unsigned filterIndex = 0;
    unsigned length = thisObj->get(exec, exec->propertyNames().length)->toUInt32(exec);
    for (unsigned k = 0; k < length && !exec->hadException(); ++k) {
        PropertySlot slot(thisObj);

        if (!thisObj->getPropertySlot(exec, k, slot))
            continue;

        JSValue* v = slot.getValue(exec, k);

        ArgList eachArguments;

        eachArguments.append(v);
        eachArguments.append(jsNumber(exec, k));
        eachArguments.append(thisObj);

        JSValue* result = call(exec, function, callType, callData, applyThis, eachArguments);

        if (result->toBoolean(exec))
            resultArray->put(exec, filterIndex++, v);
    }
    return resultArray;
}

JSValue* arrayProtoFuncMap(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    JSObject* thisObj = thisValue->toThisObject(exec);

    JSValue* function = args[0];
    CallData callData;
    CallType callType = function->getCallData(callData);
    if (callType == CallTypeNone)
        return throwError(exec, TypeError);

    JSObject* applyThis = args[1]->isUndefinedOrNull() ? exec->globalThisValue() : args[1]->toObject(exec);

    unsigned length = thisObj->get(exec, exec->propertyNames().length)->toUInt32(exec);

    JSArray* resultArray = constructEmptyArray(exec, length);

    for (unsigned k = 0; k < length && !exec->hadException(); ++k) {
        PropertySlot slot(thisObj);
        if (!thisObj->getPropertySlot(exec, k, slot))
            continue;

        JSValue* v = slot.getValue(exec, k);

        ArgList eachArguments;

        eachArguments.append(v);
        eachArguments.append(jsNumber(exec, k));
        eachArguments.append(thisObj);

        JSValue* result = call(exec, function, callType, callData, applyThis, eachArguments);
        resultArray->put(exec, k, result);
    }

    return resultArray;
}

// Documentation for these three is available at:
// http://developer-test.mozilla.org/en/docs/Core_JavaScript_1.5_Reference:Objects:Array:every
// http://developer-test.mozilla.org/en/docs/Core_JavaScript_1.5_Reference:Objects:Array:forEach
// http://developer-test.mozilla.org/en/docs/Core_JavaScript_1.5_Reference:Objects:Array:some

JSValue* arrayProtoFuncEvery(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    JSObject* thisObj = thisValue->toThisObject(exec);

    JSValue* function = args[0];
    CallData callData;
    CallType callType = function->getCallData(callData);
    if (callType == CallTypeNone)
        return throwError(exec, TypeError);

    JSObject* applyThis = args[1]->isUndefinedOrNull() ? exec->globalThisValue() : args[1]->toObject(exec);

    JSValue* result = jsBoolean(true);

    unsigned length = thisObj->get(exec, exec->propertyNames().length)->toUInt32(exec);
    for (unsigned k = 0; k < length && !exec->hadException(); ++k) {
        PropertySlot slot(thisObj);

        if (!thisObj->getPropertySlot(exec, k, slot))
            continue;

        ArgList eachArguments;

        eachArguments.append(slot.getValue(exec, k));
        eachArguments.append(jsNumber(exec, k));
        eachArguments.append(thisObj);

        bool predicateResult = call(exec, function, callType, callData, applyThis, eachArguments)->toBoolean(exec);

        if (!predicateResult) {
            result = jsBoolean(false);
            break;
        }
    }

    return result;
}

JSValue* arrayProtoFuncForEach(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    JSObject* thisObj = thisValue->toThisObject(exec);

    JSValue* function = args[0];
    CallData callData;
    CallType callType = function->getCallData(callData);
    if (callType == CallTypeNone)
        return throwError(exec, TypeError);

    JSObject* applyThis = args[1]->isUndefinedOrNull() ? exec->globalThisValue() : args[1]->toObject(exec);

    unsigned length = thisObj->get(exec, exec->propertyNames().length)->toUInt32(exec);
    for (unsigned k = 0; k < length && !exec->hadException(); ++k) {
        PropertySlot slot(thisObj);
        if (!thisObj->getPropertySlot(exec, k, slot))
            continue;

        ArgList eachArguments;
        eachArguments.append(slot.getValue(exec, k));
        eachArguments.append(jsNumber(exec, k));
        eachArguments.append(thisObj);

        call(exec, function, callType, callData, applyThis, eachArguments);
    }
    return jsUndefined();
}

JSValue* arrayProtoFuncSome(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    JSObject* thisObj = thisValue->toThisObject(exec);

    JSValue* function = args[0];
    CallData callData;
    CallType callType = function->getCallData(callData);
    if (callType == CallTypeNone)
        return throwError(exec, TypeError);

    JSObject* applyThis = args[1]->isUndefinedOrNull() ? exec->globalThisValue() : args[1]->toObject(exec);

    JSValue* result = jsBoolean(false);

    unsigned length = thisObj->get(exec, exec->propertyNames().length)->toUInt32(exec);
    for (unsigned k = 0; k < length && !exec->hadException(); ++k) {
        PropertySlot slot(thisObj);
        if (!thisObj->getPropertySlot(exec, k, slot))
            continue;

        ArgList eachArguments;
        eachArguments.append(slot.getValue(exec, k));
        eachArguments.append(jsNumber(exec, k));
        eachArguments.append(thisObj);

        bool predicateResult = call(exec, function, callType, callData, applyThis, eachArguments)->toBoolean(exec);

        if (predicateResult) {
            result = jsBoolean(true);
            break;
        }
    }
    return result;
}

JSValue* arrayProtoFuncIndexOf(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    // JavaScript 1.5 Extension by Mozilla
    // Documentation: http://developer.mozilla.org/en/docs/Core_JavaScript_1.5_Reference:Global_Objects:Array:indexOf

    JSObject* thisObj = thisValue->toThisObject(exec);

    unsigned index = 0;
    double d = args[1]->toInteger(exec);
    unsigned length = thisObj->get(exec, exec->propertyNames().length)->toUInt32(exec);
    if (d < 0)
        d += length;
    if (d > 0) {
        if (d > length)
            index = length;
        else
            index = static_cast<unsigned>(d);
    }

    JSValue* searchElement = args[0];
    for (; index < length; ++index) {
        JSValue* e = getProperty(exec, thisObj, index);
        if (!e)
            continue;
        if (strictEqual(searchElement, e))
            return jsNumber(exec, index);
    }

    return jsNumber(exec, -1);
}

JSValue* arrayProtoFuncLastIndexOf(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    // JavaScript 1.6 Extension by Mozilla
    // Documentation: http://developer.mozilla.org/en/docs/Core_JavaScript_1.5_Reference:Global_Objects:Array:lastIndexOf

    JSObject* thisObj = thisValue->toThisObject(exec);

    unsigned length = thisObj->get(exec, exec->propertyNames().length)->toUInt32(exec);
    int index = length - 1;
    double d = args[1]->toIntegerPreserveNaN(exec);

    if (d < 0) {
        d += length;
        if (d < 0)
            return jsNumber(exec, -1);
    }
    if (d < length)
        index = static_cast<int>(d);

    JSValue* searchElement = args[0];
    for (; index >= 0; --index) {
        JSValue* e = getProperty(exec, thisObj, index);
        if (!e)
            continue;
        if (strictEqual(searchElement, e))
            return jsNumber(exec, index);
    }

    return jsNumber(exec, -1);
}

}
