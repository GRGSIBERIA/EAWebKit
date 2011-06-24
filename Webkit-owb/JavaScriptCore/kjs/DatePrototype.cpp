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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 *
 */

#include "config.h"
#include "DatePrototype.h"

#include "DateMath.h"
#include "JSString.h"
#include "ObjectPrototype.h"
#include "date_object.h"
#include <float.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <time.h>
#include <wtf/Assertions.h>
#include <wtf/MathExtras.h>
#include <wtf/StringExtras.h>
#include <wtf/UnusedParam.h>

#if HAVE(SYS_PARAM_H)
#include <sys/param.h>
#endif

#if HAVE(SYS_TIME_H)
#include <sys/time.h>
#endif

#if HAVE(SYS_TIMEB_H)
#include <sys/timeb.h>
#endif

#if PLATFORM(MAC)
#include <CoreFoundation/CoreFoundation.h>
#endif

using namespace WTF;

namespace KJS {

static JSValue* dateProtoFuncGetDate(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncGetDay(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncGetFullYear(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncGetHours(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncGetMilliSeconds(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncGetMinutes(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncGetMonth(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncGetSeconds(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncGetTime(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncGetTimezoneOffset(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncGetUTCDate(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncGetUTCDay(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncGetUTCFullYear(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncGetUTCHours(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncGetUTCMilliseconds(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncGetUTCMinutes(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncGetUTCMonth(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncGetUTCSeconds(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncGetYear(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncSetDate(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncSetFullYear(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncSetHours(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncSetMilliSeconds(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncSetMinutes(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncSetMonth(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncSetSeconds(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncSetTime(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncSetUTCDate(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncSetUTCFullYear(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncSetUTCHours(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncSetUTCMilliseconds(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncSetUTCMinutes(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncSetUTCMonth(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncSetUTCSeconds(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncSetYear(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncToDateString(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncToGMTString(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncToLocaleDateString(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncToLocaleString(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncToLocaleTimeString(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncToString(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncToTimeString(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncToUTCString(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* dateProtoFuncValueOf(ExecState*, JSObject*, JSValue*, const ArgList&);

}

#include "DatePrototype.lut.h"

namespace KJS {

#if PLATFORM(MAC)

static CFDateFormatterStyle styleFromArgString(const UString& string, CFDateFormatterStyle defaultStyle)
{
    if (string == "short")
        return kCFDateFormatterShortStyle;
    if (string == "medium")
        return kCFDateFormatterMediumStyle;
    if (string == "long")
        return kCFDateFormatterLongStyle;
    if (string == "full")
        return kCFDateFormatterFullStyle;
    return defaultStyle;
}

static UString formatLocaleDate(ExecState *exec, double time, bool includeDate, bool includeTime, const ArgList& args)
{
    CFDateFormatterStyle dateStyle = (includeDate ? kCFDateFormatterLongStyle : kCFDateFormatterNoStyle);
    CFDateFormatterStyle timeStyle = (includeTime ? kCFDateFormatterLongStyle : kCFDateFormatterNoStyle);

    bool useCustomFormat = false;
    UString customFormatString;

    UString arg0String = args[0]->toString(exec);
    if (arg0String == "custom" && !args[1]->isUndefined()) {
        useCustomFormat = true;
        customFormatString = args[1]->toString(exec);
    } else if (includeDate && includeTime && !args[1]->isUndefined()) {
        dateStyle = styleFromArgString(arg0String, dateStyle);
        timeStyle = styleFromArgString(args[1]->toString(exec), timeStyle);
    } else if (includeDate && !args[0]->isUndefined()) {
        dateStyle = styleFromArgString(arg0String, dateStyle);
    } else if (includeTime && !args[0]->isUndefined()) {
        timeStyle = styleFromArgString(arg0String, timeStyle);
    }

    CFLocaleRef locale = CFLocaleCopyCurrent();
    CFDateFormatterRef formatter = CFDateFormatterCreate(0, locale, dateStyle, timeStyle);
    CFRelease(locale);

    if (useCustomFormat) {
        CFStringRef customFormatCFString = CFStringCreateWithCharacters(0, (UniChar *)customFormatString.data(), customFormatString.size());
        CFDateFormatterSetFormat(formatter, customFormatCFString);
        CFRelease(customFormatCFString);
    }

    CFStringRef string = CFDateFormatterCreateStringWithAbsoluteTime(0, formatter, time - kCFAbsoluteTimeIntervalSince1970);

    CFRelease(formatter);

    // We truncate the string returned from CFDateFormatter if it's absurdly long (> 200 characters).
    // That's not great error handling, but it just won't happen so it doesn't matter.
    UChar buffer[200];
    const size_t bufferLength = sizeof(buffer) / sizeof(buffer[0]);
    size_t length = CFStringGetLength(string);
    ASSERT(length <= bufferLength);
    if (length > bufferLength)
        length = bufferLength;
    CFStringGetCharacters(string, CFRangeMake(0, length), reinterpret_cast<UniChar *>(buffer));

    CFRelease(string);

    return UString(buffer, length);
}

#else

enum LocaleDateTimeFormat { LocaleDateAndTime, LocaleDate, LocaleTime };
 
static JSCell* formatLocaleDate(ExecState* exec, const GregorianDateTime& gdt, const LocaleDateTimeFormat format)
{
    static const char* formatStrings[] = {"%#c", "%#x", "%X"};
 
    // Offset year if needed
    struct tm localTM = gdt;
    int year = gdt.year + 1900;
    bool yearNeedsOffset = year < 1900 || year > 2038;
    if (yearNeedsOffset) {
        localTM.tm_year = equivalentYearForDST(year) - 1900;
     }
 
    // Do the formatting
    const int bufsize=128;
    char timebuffer[bufsize];
    size_t ret = strftime(timebuffer, bufsize, formatStrings[format], &localTM);
 
    if ( ret == 0 )
        return jsString(exec, "");
 
    // Copy original into the buffer
    if (yearNeedsOffset && format != LocaleTime) {
        static const int yearLen = 5;   // FIXME will be a problem in the year 10,000
        char yearString[yearLen];
 
        snprintf(yearString, yearLen, "%d", localTM.tm_year + 1900);
        char* yearLocation = strstr(timebuffer, yearString);
        snprintf(yearString, yearLen, "%d", year);
 
        strncpy(yearLocation, yearString, yearLen - 1);
    }
 
    return jsString(exec, timebuffer);
}

#endif // PLATFORM(WIN_OS)

// Converts a list of arguments sent to a Date member function into milliseconds, updating
// ms (representing milliseconds) and t (representing the rest of the date structure) appropriately.
//
// Format of member function: f([hour,] [min,] [sec,] [ms])
static bool fillStructuresUsingTimeArgs(ExecState* exec, const ArgList& args, int maxArgs, double* ms, GregorianDateTime* t)
{
    double milliseconds = 0;
    bool ok = true;
    int idx = 0;
    int numArgs = args.size();
    
    // JS allows extra trailing arguments -- ignore them
    if (numArgs > maxArgs)
        numArgs = maxArgs;

    // hours
    if (maxArgs >= 4 && idx < numArgs) {
        t->hour = 0;
        milliseconds += args[idx++]->toInt32(exec, ok) * msPerHour;
    }

    // minutes
    if (maxArgs >= 3 && idx < numArgs && ok) {
        t->minute = 0;
        milliseconds += args[idx++]->toInt32(exec, ok) * msPerMinute;
    }
    
    // seconds
    if (maxArgs >= 2 && idx < numArgs && ok) {
        t->second = 0;
        milliseconds += args[idx++]->toInt32(exec, ok) * msPerSecond;
    }
    
    if (!ok)
        return false;
        
    // milliseconds
    if (idx < numArgs) {
        double millis = args[idx]->toNumber(exec);
        ok = isfinite(millis);
        milliseconds += millis;
    } else
        milliseconds += *ms;
    
    *ms = milliseconds;
    return ok;
}

// Converts a list of arguments sent to a Date member function into years, months, and milliseconds, updating
// ms (representing milliseconds) and t (representing the rest of the date structure) appropriately.
//
// Format of member function: f([years,] [months,] [days])
static bool fillStructuresUsingDateArgs(ExecState *exec, const ArgList& args, int maxArgs, double *ms, GregorianDateTime *t)
{
    int idx = 0;
    bool ok = true;
    int numArgs = args.size();
  
    // JS allows extra trailing arguments -- ignore them
    if (numArgs > maxArgs)
        numArgs = maxArgs;
  
    // years
    if (maxArgs >= 3 && idx < numArgs)
        t->year = args[idx++]->toInt32(exec, ok) - 1900;
    
    // months
    if (maxArgs >= 2 && idx < numArgs && ok)   
        t->month = args[idx++]->toInt32(exec, ok);
    
    // days
    if (idx < numArgs && ok) {   
        t->monthDay = 0;
        *ms += args[idx]->toInt32(exec, ok) * msPerDay;
    }
    
    return ok;
}





const ClassInfo DatePrototype::info = {"Date", &DateInstance::info, 0, ExecState::dateTable};

/* Source for date_object.lut.h
   FIXME: We could use templates to simplify the UTC variants.
@begin dateTable
  toString              dateProtoFuncToString                DontEnum|Function       0
  toUTCString           dateProtoFuncToUTCString             DontEnum|Function       0
  toDateString          dateProtoFuncToDateString            DontEnum|Function       0
  toTimeString          dateProtoFuncToTimeString            DontEnum|Function       0
  toLocaleString        dateProtoFuncToLocaleString          DontEnum|Function       0
  toLocaleDateString    dateProtoFuncToLocaleDateString      DontEnum|Function       0
  toLocaleTimeString    dateProtoFuncToLocaleTimeString      DontEnum|Function       0
  valueOf               dateProtoFuncValueOf                 DontEnum|Function       0
  getTime               dateProtoFuncGetTime                 DontEnum|Function       0
  getFullYear           dateProtoFuncGetFullYear             DontEnum|Function       0
  getUTCFullYear        dateProtoFuncGetUTCFullYear          DontEnum|Function       0
  toGMTString           dateProtoFuncToGMTString             DontEnum|Function       0
  getMonth              dateProtoFuncGetMonth                DontEnum|Function       0
  getUTCMonth           dateProtoFuncGetUTCMonth             DontEnum|Function       0
  getDate               dateProtoFuncGetDate                 DontEnum|Function       0
  getUTCDate            dateProtoFuncGetUTCDate              DontEnum|Function       0
  getDay                dateProtoFuncGetDay                  DontEnum|Function       0
  getUTCDay             dateProtoFuncGetUTCDay               DontEnum|Function       0
  getHours              dateProtoFuncGetHours                DontEnum|Function       0
  getUTCHours           dateProtoFuncGetUTCHours             DontEnum|Function       0
  getMinutes            dateProtoFuncGetMinutes              DontEnum|Function       0
  getUTCMinutes         dateProtoFuncGetUTCMinutes           DontEnum|Function       0
  getSeconds            dateProtoFuncGetSeconds              DontEnum|Function       0
  getUTCSeconds         dateProtoFuncGetUTCSeconds           DontEnum|Function       0
  getMilliseconds       dateProtoFuncGetMilliSeconds         DontEnum|Function       0
  getUTCMilliseconds    dateProtoFuncGetUTCMilliseconds      DontEnum|Function       0
  getTimezoneOffset     dateProtoFuncGetTimezoneOffset       DontEnum|Function       0
  setTime               dateProtoFuncSetTime                 DontEnum|Function       1
  setMilliseconds       dateProtoFuncSetMilliSeconds         DontEnum|Function       1
  setUTCMilliseconds    dateProtoFuncSetUTCMilliseconds      DontEnum|Function       1
  setSeconds            dateProtoFuncSetSeconds              DontEnum|Function       2
  setUTCSeconds         dateProtoFuncSetUTCSeconds           DontEnum|Function       2
  setMinutes            dateProtoFuncSetMinutes              DontEnum|Function       3
  setUTCMinutes         dateProtoFuncSetUTCMinutes           DontEnum|Function       3
  setHours              dateProtoFuncSetHours                DontEnum|Function       4
  setUTCHours           dateProtoFuncSetUTCHours             DontEnum|Function       4
  setDate               dateProtoFuncSetDate                 DontEnum|Function       1
  setUTCDate            dateProtoFuncSetUTCDate              DontEnum|Function       1
  setMonth              dateProtoFuncSetMonth                DontEnum|Function       2
  setUTCMonth           dateProtoFuncSetUTCMonth             DontEnum|Function       2
  setFullYear           dateProtoFuncSetFullYear             DontEnum|Function       3
  setUTCFullYear        dateProtoFuncSetUTCFullYear          DontEnum|Function       3
  setYear               dateProtoFuncSetYear                 DontEnum|Function       1
  getYear               dateProtoFuncGetYear                 DontEnum|Function       0
@end
*/
// ECMA 15.9.4

DatePrototype::DatePrototype(ExecState* exec, ObjectPrototype* objectProto)
    : DateInstance(objectProto)
{
    setInternalValue(jsNaN(exec));
    // The constructor will be added later, after DateConstructor has been built.
}

bool DatePrototype::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticFunctionSlot<JSObject>(exec, ExecState::dateTable(exec), this, propertyName, slot);
}

// Functions

JSValue* dateProtoFuncToString(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = false;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsString(exec, "Invalid Date");

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsString(exec, formatDate(t) + " " + formatTime(t, utc));
}

JSValue* dateProtoFuncToUTCString(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = true;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsString(exec, "Invalid Date");

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsString(exec, formatDateUTCVariant(t) + " " + formatTime(t, utc));
}

JSValue* dateProtoFuncToDateString(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = false;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsString(exec, "Invalid Date");

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsString(exec, formatDate(t));
}

JSValue* dateProtoFuncToTimeString(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = false;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsString(exec, "Invalid Date");

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsString(exec, formatTime(t, utc));
}

JSValue* dateProtoFuncToLocaleString(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsString(exec, "Invalid Date");

#if PLATFORM(MAC)
    double secs = floor(milli / msPerSecond);
    return jsString(exec, formatLocaleDate(exec, secs, true, true, args));
#else
    UNUSED_PARAM(args);

    const bool utc = false;

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return formatLocaleDate(exec, t, LocaleDateAndTime);
#endif
}

JSValue* dateProtoFuncToLocaleDateString(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsString(exec, "Invalid Date");

#if PLATFORM(MAC)
    double secs = floor(milli / msPerSecond);
    return jsString(exec, formatLocaleDate(exec, secs, true, false, args));
#else
    UNUSED_PARAM(args);

    const bool utc = false;

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return formatLocaleDate(exec, t, LocaleDate);
#endif
}

JSValue* dateProtoFuncToLocaleTimeString(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsString(exec, "Invalid Date");

#if PLATFORM(MAC)
    double secs = floor(milli / msPerSecond);
    return jsString(exec, formatLocaleDate(exec, secs, false, true, args));
#else
    UNUSED_PARAM(args);

    const bool utc = false;

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return formatLocaleDate(exec, t, LocaleTime);
#endif
}

JSValue* dateProtoFuncValueOf(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    return jsNumber(exec, milli);
}

JSValue* dateProtoFuncGetTime(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    return jsNumber(exec, milli);
}

JSValue* dateProtoFuncGetFullYear(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = false;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, 1900 + t.year);
}

JSValue* dateProtoFuncGetUTCFullYear(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = true;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, 1900 + t.year);
}

JSValue* dateProtoFuncToGMTString(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = true;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsString(exec, "Invalid Date");

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsString(exec, formatDateUTCVariant(t) + " " + formatTime(t, utc));
}

JSValue* dateProtoFuncGetMonth(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = false;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, t.month);
}

JSValue* dateProtoFuncGetUTCMonth(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = true;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, t.month);
}

JSValue* dateProtoFuncGetDate(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = false;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, t.monthDay);
}

JSValue* dateProtoFuncGetUTCDate(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = true;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, t.monthDay);
}

JSValue* dateProtoFuncGetDay(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = false;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, t.weekDay);
}

JSValue* dateProtoFuncGetUTCDay(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = true;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, t.weekDay);
}

JSValue* dateProtoFuncGetHours(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = false;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, t.hour);
}

JSValue* dateProtoFuncGetUTCHours(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = true;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, t.hour);
}

JSValue* dateProtoFuncGetMinutes(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = false;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, t.minute);
}

JSValue* dateProtoFuncGetUTCMinutes(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = true;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, t.minute);
}

JSValue* dateProtoFuncGetSeconds(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = false;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, t.second);
}

JSValue* dateProtoFuncGetUTCSeconds(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = true;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, t.second);
}

JSValue* dateProtoFuncGetMilliSeconds(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    double secs = floor(milli / msPerSecond);
    double ms = milli - secs * msPerSecond;
    return jsNumber(exec, ms);
}

JSValue* dateProtoFuncGetUTCMilliseconds(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    double secs = floor(milli / msPerSecond);
    double ms = milli - secs * msPerSecond;
    return jsNumber(exec, ms);
}

JSValue* dateProtoFuncGetTimezoneOffset(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = false;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);
    return jsNumber(exec, -gmtoffset(t) / minutesPerHour);
}

JSValue* dateProtoFuncSetTime(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 

    double milli = timeClip(args[0]->toNumber(exec));
    JSValue* result = jsNumber(exec, milli);
    thisDateObj->setInternalValue(result);
    return result;
}

static JSValue* setNewValueFromTimeArgs(ExecState* exec, JSValue* thisValue, const ArgList& args, int numArgsToUse, bool inputIsUTC)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue);
    double milli = thisDateObj->internalNumber();
    
    if (args.isEmpty() || isnan(milli)) {
        JSValue* result = jsNaN(exec);
        thisDateObj->setInternalValue(result);
        return result;
    }
     
    double secs = floor(milli / msPerSecond);
    double ms = milli - secs * msPerSecond;

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, inputIsUTC, t);

    if (!fillStructuresUsingTimeArgs(exec, args, numArgsToUse, &ms, &t)) {
        JSValue* result = jsNaN(exec);
        thisDateObj->setInternalValue(result);
        return result;
    } 
    
    JSValue* result = jsNumber(exec, gregorianDateTimeToMS(t, ms, inputIsUTC));
    thisDateObj->setInternalValue(result);
    return result;
}

static JSValue* setNewValueFromDateArgs(ExecState* exec, JSValue* thisValue, const ArgList& args, int numArgsToUse, bool inputIsUTC)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue);
    if (args.isEmpty()) {
        JSValue* result = jsNaN(exec);
        thisDateObj->setInternalValue(result);
        return result;
    }      
    
    double milli = thisDateObj->internalNumber();
    double ms = 0;

    GregorianDateTime t;
    if (numArgsToUse == 3 && isnan(milli))
        // Based on ECMA 262 15.9.5.40 - .41 (set[UTC]FullYear)
        // the time must be reset to +0 if it is NaN. 
        thisDateObj->msToGregorianDateTime(0, true, t);
    else {
        double secs = floor(milli / msPerSecond);
        ms = milli - secs * msPerSecond;
        thisDateObj->msToGregorianDateTime(milli, inputIsUTC, t);
    }
    
    if (!fillStructuresUsingDateArgs(exec, args, numArgsToUse, &ms, &t)) {
        JSValue* result = jsNaN(exec);
        thisDateObj->setInternalValue(result);
        return result;
    } 
           
    JSValue* result = jsNumber(exec, gregorianDateTimeToMS(t, ms, inputIsUTC));
    thisDateObj->setInternalValue(result);
    return result;
}

JSValue* dateProtoFuncSetMilliSeconds(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    const bool inputIsUTC = false;
    return setNewValueFromTimeArgs(exec, thisValue, args, 1, inputIsUTC);
}

JSValue* dateProtoFuncSetUTCMilliseconds(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    const bool inputIsUTC = true;
    return setNewValueFromTimeArgs(exec, thisValue, args, 1, inputIsUTC);
}

JSValue* dateProtoFuncSetSeconds(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    const bool inputIsUTC = false;
    return setNewValueFromTimeArgs(exec, thisValue, args, 2, inputIsUTC);
}

JSValue* dateProtoFuncSetUTCSeconds(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    const bool inputIsUTC = true;
    return setNewValueFromTimeArgs(exec, thisValue, args, 2, inputIsUTC);
}

JSValue* dateProtoFuncSetMinutes(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    const bool inputIsUTC = false;
    return setNewValueFromTimeArgs(exec, thisValue, args, 3, inputIsUTC);
}

JSValue* dateProtoFuncSetUTCMinutes(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    const bool inputIsUTC = true;
    return setNewValueFromTimeArgs(exec, thisValue, args, 3, inputIsUTC);
}

JSValue* dateProtoFuncSetHours(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    const bool inputIsUTC = false;
    return setNewValueFromTimeArgs(exec, thisValue, args, 4, inputIsUTC);
}

JSValue* dateProtoFuncSetUTCHours(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    const bool inputIsUTC = true;
    return setNewValueFromTimeArgs(exec, thisValue, args, 4, inputIsUTC);
}

JSValue* dateProtoFuncSetDate(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    const bool inputIsUTC = false;
    return setNewValueFromDateArgs(exec, thisValue, args, 1, inputIsUTC);
}

JSValue* dateProtoFuncSetUTCDate(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    const bool inputIsUTC = true;
    return setNewValueFromDateArgs(exec, thisValue, args, 1, inputIsUTC);
}

JSValue* dateProtoFuncSetMonth(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    const bool inputIsUTC = false;
    return setNewValueFromDateArgs(exec, thisValue, args, 2, inputIsUTC);
}

JSValue* dateProtoFuncSetUTCMonth(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    const bool inputIsUTC = true;
    return setNewValueFromDateArgs(exec, thisValue, args, 2, inputIsUTC);
}

JSValue* dateProtoFuncSetFullYear(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    const bool inputIsUTC = false;
    return setNewValueFromDateArgs(exec, thisValue, args, 3, inputIsUTC);
}

JSValue* dateProtoFuncSetUTCFullYear(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    const bool inputIsUTC = true;
    return setNewValueFromDateArgs(exec, thisValue, args, 3, inputIsUTC);
}

JSValue* dateProtoFuncSetYear(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = false;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue);     
    if (args.isEmpty()) { 
        JSValue* result = jsNaN(exec);
        thisDateObj->setInternalValue(result);
        return result;
    }
    
    double milli = thisDateObj->internalNumber();
    double ms = 0;

    GregorianDateTime t;
    if (isnan(milli))
        // Based on ECMA 262 B.2.5 (setYear)
        // the time must be reset to +0 if it is NaN. 
        thisDateObj->msToGregorianDateTime(0, true, t);
    else {   
        double secs = floor(milli / msPerSecond);
        ms = milli - secs * msPerSecond;
        thisDateObj->msToGregorianDateTime(milli, utc, t);
    }
    
    bool ok = true;
    int32_t year = args[0]->toInt32(exec, ok);
    if (!ok) {
        JSValue* result = jsNaN(exec);
        thisDateObj->setInternalValue(result);
        return result;
    }
            
    t.year = (year > 99 || year < 0) ? year - 1900 : year;
    JSValue* result = jsNumber(exec, gregorianDateTimeToMS(t, ms, utc));
    thisDateObj->setInternalValue(result);
    return result;
}

JSValue* dateProtoFuncGetYear(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&DateInstance::info))
        return throwError(exec, TypeError);

    const bool utc = false;

    DateInstance* thisDateObj = static_cast<DateInstance*>(thisValue); 
    double milli = thisDateObj->internalNumber();
    if (isnan(milli))
        return jsNaN(exec);

    GregorianDateTime t;
    thisDateObj->msToGregorianDateTime(milli, utc, t);

    // NOTE: IE returns the full year even in getYear.
    return jsNumber(exec, t.year);
}

} // namespace KJS
