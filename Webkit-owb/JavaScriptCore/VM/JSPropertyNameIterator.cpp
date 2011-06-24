/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
* This file was modified by Electronic Arts Inc Copyright � 2009
*/

#include "config.h"
#include "JSPropertyNameIterator.h"

#include "JSObject.h"
#include "JSString.h"
#include "PropertyNameArray.h"
#include "identifier.h"

namespace KJS {

COMPILE_ASSERT(sizeof(JSPropertyNameIterator) <= CellSize<sizeof(void*)>::m_value, JSPropertyNameIteratorSizeASSERT);

JSPropertyNameIterator* JSPropertyNameIterator::create(ExecState* exec, JSValue* v)
{
    if (v->isUndefinedOrNull())
        return new (exec) JSPropertyNameIterator(0, 0, 0);

    JSObject* o = v->toObject(exec);
    PropertyNameArray propertyNames(exec);
    o->getPropertyNames(exec, propertyNames);
    size_t numProperties = propertyNames.size();
    return new (exec) JSPropertyNameIterator(o, propertyNames.releaseIdentifiers(), numProperties);
}

JSPropertyNameIterator::JSPropertyNameIterator(JSObject* object, Identifier* propertyNames, size_t numProperties)
    : m_object(object)
    , m_propertyNames(propertyNames)
    , m_position(propertyNames)
    , m_end(propertyNames + numProperties)
{
}

JSPropertyNameIterator::~JSPropertyNameIterator()
{
    invalidate();               // 4/20/09 CSidhall - Call to clean up m_propertyNames (delete does not clean up the array in this case)
    //delete m_propertyNames;
}

JSType JSPropertyNameIterator::type() const
{
    return UnspecifiedType;
}

JSValue* JSPropertyNameIterator::toPrimitive(ExecState*, JSType) const
{
    ASSERT_NOT_REACHED();
    return 0;
}

bool JSPropertyNameIterator::getPrimitiveNumber(ExecState*, double&, JSValue*&)
{
    ASSERT_NOT_REACHED();
    return false;
}

bool JSPropertyNameIterator::toBoolean(ExecState*) const
{
    ASSERT_NOT_REACHED();
    return false;
}

double JSPropertyNameIterator::toNumber(ExecState*) const
{
    ASSERT_NOT_REACHED();
    return 0;
}

UString JSPropertyNameIterator::toString(ExecState*) const
{
    ASSERT_NOT_REACHED();
    return "";
}

JSObject* JSPropertyNameIterator::toObject(ExecState*) const
{
    ASSERT_NOT_REACHED();
    return 0;
}

void JSPropertyNameIterator::mark()
{
    JSCell::mark();
    if (m_object && !m_object->marked())
        m_object->mark();
}

JSValue* JSPropertyNameIterator::next(ExecState* exec)
{
    while (m_position != m_end) {
        if (m_object->hasProperty(exec, *m_position))
            return jsOwnedString(exec, (*m_position++).ustring());;
        m_position++;
    }
    invalidate();
    return 0;
}

void JSPropertyNameIterator::invalidate()
{
    // 4/20/09 CSidhall - Leak fix.
    // The identifier buffer was actually copied from a vector array and then set the m_size of that array 0 (so destructors not called).
    // The delete here cleaned up the full memory buffer but only called the destructor on the first element in the original array, leaving
    // the others to leak...
    if(m_propertyNames) {
        for( Identifier *it = m_propertyNames; it < m_end; it++)
        {
            it->~Identifier();
        }
        fastFree(m_propertyNames);
        m_end = 0;
    }
    // Old code used a delete:
    //delete m_propertyNames;

    m_object = 0;
    m_propertyNames = 0;
}

} // namespace KJS
