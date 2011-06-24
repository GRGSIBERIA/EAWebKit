/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

/*
* This file was modified by Electronic Arts Inc Copyright � 2009
*/

#include "config.h"

#ifdef AVOID_STATIC_CONSTRUCTORS
#define ATOMICSTRING_HIDE_GLOBALS 1
#endif

#include "AtomicString.h"

#include "StaticConstructors.h"
#include "StringHash.h"
#include <kjs/identifier.h>
#include <wtf/HashSet.h>

using KJS::Identifier;
using KJS::UString;

namespace WebCore {

static HashSet<StringImpl*>* stringTable;
#include <wtf/FastAllocBase.h>

struct CStringTranslator: public WTF::FastAllocBase {
    static unsigned hash(const char* c)
    {
        return StringImpl::computeHash(c);
    }

    static bool equal(StringImpl* r, const char* s)
    {
        int length = r->length();
        const UChar* d = r->characters();
        for (int i = 0; i != length; ++i) {
            unsigned char c = s[i];
            if (d[i] != c)
                return false;
        }
        return s[length] == 0;
    }

    static void translate(StringImpl*& location, const char* const& c, unsigned hash)
    {
        location = new StringImpl(c, strlen(c), hash); 
    }
};

bool operator==(const AtomicString& a, const char* b)
{ 
    StringImpl* impl = a.impl();
    if ((!impl || !impl->characters()) && !b)
        return true;
    if ((!impl || !impl->characters()) || !b)
        return false;
    return CStringTranslator::equal(impl, b); 
}

PassRefPtr<StringImpl> AtomicString::add(const char* c)
{
    if (!c)
        return 0;
    if (!*c)
        return StringImpl::empty();    
    pair<HashSet<StringImpl*>::iterator, bool> addResult = stringTable->add<const char*, CStringTranslator>(c);
    if (!addResult.second)
        return *addResult.first;
    return adoptRef(*addResult.first);
}

struct UCharBuffer: public WTF::FastAllocBase {
    const UChar* s;
    unsigned length;
};

static inline bool equal(StringImpl* string, const UChar* characters, unsigned length)
{
    if (string->length() != length)
        return false;

#if PLATFORM(ARM)
    const UChar* stringCharacters = string->characters();
    for (unsigned i = 0; i != length; ++i) {
        if (*stringCharacters++ != *characters++)
            return false;
    }
    return true;
#else
    /* Do it 4-bytes-at-a-time on architectures where it's safe */

    const uint32_t* stringCharacters = reinterpret_cast<const uint32_t*>(string->characters());
    const uint32_t* bufferCharacters = reinterpret_cast<const uint32_t*>(characters);

    unsigned halfLength = length >> 1;
    for (unsigned i = 0; i != halfLength; ++i) {
        if (*stringCharacters++ != *bufferCharacters++)
            return false;
    }

    if (length & 1 &&  *reinterpret_cast<const uint16_t*>(stringCharacters) != *reinterpret_cast<const uint16_t*>(bufferCharacters))
        return false;

    return true;
#endif
}

struct UCharBufferTranslator: public WTF::FastAllocBase {
    static unsigned hash(const UCharBuffer& buf)
    {
        return StringImpl::computeHash(buf.s, buf.length);
    }

    static bool equal(StringImpl* const& str, const UCharBuffer& buf)
    {
        return WebCore::equal(str, buf.s, buf.length);
    }

    static void translate(StringImpl*& location, const UCharBuffer& buf, unsigned hash)
    {
        location = new StringImpl(buf.s, buf.length, hash); 
    }
};

struct HashAndCharacters {
    unsigned hash;
    const UChar* characters;
    unsigned length;
};

struct HashAndCharactersTranslator {
    static unsigned hash(const HashAndCharacters& buffer)
    {
        ASSERT(buffer.hash == StringImpl::computeHash(buffer.characters, buffer.length));
        return buffer.hash;
    }

    static bool equal(StringImpl* const& string, const HashAndCharacters& buffer)
    {
        return WebCore::equal(string, buffer.characters, buffer.length);
    }

    static void translate(StringImpl*& location, const HashAndCharacters& buffer, unsigned hash)
    {
        location = new StringImpl(buffer.characters, buffer.length, hash); 
    }
};

PassRefPtr<StringImpl> AtomicString::add(const UChar* s, int length)
{
    if (!s)
        return 0;

    if (length == 0)
        return StringImpl::empty();
    
    UCharBuffer buf = { s, length }; 
    pair<HashSet<StringImpl*>::iterator, bool> addResult = stringTable->add<UCharBuffer, UCharBufferTranslator>(buf);
    if (!addResult.second)
        return *addResult.first;
    return adoptRef(*addResult.first);
}

PassRefPtr<StringImpl> AtomicString::add(const UChar* s)
{
    if (!s)
        return 0;

    int length = 0;
    while (s[length] != UChar(0))
        length++;

    if (length == 0)
        return StringImpl::empty();

    UCharBuffer buf = {s, length}; 
    pair<HashSet<StringImpl*>::iterator, bool> addResult = stringTable->add<UCharBuffer, UCharBufferTranslator>(buf);
    if (!addResult.second)
        return *addResult.first;
    return adoptRef(*addResult.first);
}

PassRefPtr<StringImpl> AtomicString::add(StringImpl* r)
{
    if (!r || r->m_inTable)
        return r;

    if (r->length() == 0)
        return StringImpl::empty();
    
    StringImpl* result = *stringTable->add(r).first;
    if (result == r)
        r->m_inTable = true;
    return result;
}

void AtomicString::remove(StringImpl* r)
{
    stringTable->remove(r);
}

PassRefPtr<StringImpl> AtomicString::add(const KJS::Identifier& identifier)
{
    if (identifier.isNull())
        return 0;

    UString::Rep* string = identifier.ustring().rep();
    unsigned length = string->size();
    if (!length)
        return StringImpl::empty();

    HashAndCharacters buffer = { string->computedHash(), string->data(), length }; 
    pair<HashSet<StringImpl*>::iterator, bool> addResult = stringTable->add<HashAndCharacters, HashAndCharactersTranslator>(buffer);
    if (!addResult.second)
        return *addResult.first;
    return adoptRef(*addResult.first);
}

PassRefPtr<StringImpl> AtomicString::add(const KJS::UString& ustring)
{
    if (ustring.isNull())
        return 0;

    UString::Rep* string = ustring.rep();
    unsigned length = string->size();
    if (!length)
        return StringImpl::empty();

    HashAndCharacters buffer = { string->hash(), string->data(), length }; 
    pair<HashSet<StringImpl*>::iterator, bool> addResult = stringTable->add<HashAndCharacters, HashAndCharactersTranslator>(buffer);
    if (!addResult.second)
        return *addResult.first;
    return adoptRef(*addResult.first);
}

AtomicStringImpl* AtomicString::find(const KJS::Identifier& identifier)
{
    if (identifier.isNull())
        return 0;

    UString::Rep* string = identifier.ustring().rep();
    unsigned length = string->size();
    if (!length)
        return static_cast<AtomicStringImpl*>(StringImpl::empty());

    HashAndCharacters buffer = { string->computedHash(), string->data(), length }; 
    HashSet<StringImpl*>::iterator iterator = stringTable->find<HashAndCharacters, HashAndCharactersTranslator>(buffer);
    if (iterator == stringTable->end())
        return 0;
    return static_cast<AtomicStringImpl*>(*iterator);
}

AtomicString::operator UString() const
{
    return m_string;
}

DEFINE_GLOBAL(AtomicString, nullAtom)
DEFINE_GLOBAL(AtomicString, emptyAtom, "")
DEFINE_GLOBAL(AtomicString, textAtom, "#text")
DEFINE_GLOBAL(AtomicString, commentAtom, "#comment")
DEFINE_GLOBAL(AtomicString, starAtom, "*")

void AtomicString::init()
{
    static bool initialized;
    if (!initialized) {
        stringTable = new HashSet<StringImpl*>;

        // Use placement new to initialize the globals.
        new ((void*)&nullAtom) AtomicString;
        new ((void*)&emptyAtom) AtomicString("");
        new ((void*)&textAtom) AtomicString("#text");
        new ((void*)&commentAtom) AtomicString("#comment");
        new ((void*)&starAtom) AtomicString("*");

        initialized = true;
    }
}

}
