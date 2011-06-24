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

/*
* This file was modified by Electronic Arts Inc Copyright � 2009
*/

#include "config.h"
#include "date_object.h"

#include "DateMath.h"
#include <math.h>
#include <wtf/MathExtras.h>
#include <wtf/FastAllocBase.h>

namespace KJS {

struct DateInstance::Cache  {
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
    double m_gregorianDateTimeCachedForMS;
    GregorianDateTime m_cachedGregorianDateTime;
    double m_gregorianDateTimeUTCCachedForMS;
    GregorianDateTime m_cachedGregorianDateTimeUTC;
};

const ClassInfo DateInstance::info = {"Date", 0, 0, 0};

DateInstance::DateInstance(JSObject *proto)
  : JSWrapperObject(proto)
  , m_cache(0)
{
}

DateInstance::~DateInstance()
{
    delete m_cache;
}

void DateInstance::msToGregorianDateTime(double milli, bool outputIsUTC, GregorianDateTime& t) const
{
    if (!m_cache) {
        m_cache = new Cache;
        m_cache->m_gregorianDateTimeCachedForMS = NaN;
        m_cache->m_gregorianDateTimeUTCCachedForMS = NaN;
    }

    if (outputIsUTC) {
        if (m_cache->m_gregorianDateTimeUTCCachedForMS != milli) {
            KJS::msToGregorianDateTime(milli, true, m_cache->m_cachedGregorianDateTimeUTC);
            m_cache->m_gregorianDateTimeUTCCachedForMS = milli;
        }
        t.copyFrom(m_cache->m_cachedGregorianDateTimeUTC);
    } else {
        if (m_cache->m_gregorianDateTimeCachedForMS != milli) {
            KJS::msToGregorianDateTime(milli, false, m_cache->m_cachedGregorianDateTime);
            m_cache->m_gregorianDateTimeCachedForMS = milli;
        }
        t.copyFrom(m_cache->m_cachedGregorianDateTime);
    }
}

bool DateInstance::getTime(GregorianDateTime &t, int &offset) const
{
    double milli = internalNumber();
    if (isnan(milli))
        return false;
    
    msToGregorianDateTime(milli, false, t);
    offset = gmtoffset(t);
    return true;
}

bool DateInstance::getUTCTime(GregorianDateTime &t) const
{
    double milli = internalNumber();
    if (isnan(milli))
        return false;
    
    msToGregorianDateTime(milli, true, t);
    return true;
}

bool DateInstance::getTime(double &milli, int &offset) const
{
    milli = internalNumber();
    if (isnan(milli))
        return false;
    
    GregorianDateTime t;
    msToGregorianDateTime(milli, false, t);
    offset = gmtoffset(t);
    return true;
}

bool DateInstance::getUTCTime(double &milli) const
{
    milli = internalNumber();
    if (isnan(milli))
        return false;
    
    return true;
}

} // namespace KJS
