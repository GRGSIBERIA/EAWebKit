// -*- c-basic-offset: 2 -*-
/*
*  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
*  Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef _KJS_USTRING_H_
#define _KJS_USTRING_H_

#include <wtf/FastAllocBase.h>
#include "JSLock.h"
#include "collector.h"
#include <stdint.h>
#include <wtf/Assertions.h>
#include <wtf/FastMalloc.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/unicode/Unicode.h>
#include <wtf/Vector.h>

/**
* @internal
*/
namespace DOM {
    class DOMString;
    class AtomicString;
}
class KJScript;

namespace KJS {

    using WTF::PlacementNewAdoptType;
    using WTF::PlacementNewAdopt;

    class IdentifierTable;
    class UString;

    /**
    * @short 8 bit char based string class
    */
  class CString {
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
        CString() : data(0), length(0) { }
        CString(const char *c);
        CString(const char *c, size_t len);
        CString(const CString &);

        ~CString();

        static CString adopt(char* c, size_t len); // c should be allocated with new[].

        CString &append(const CString &);
        CString &operator=(const char *c);
        CString &operator=(const CString &);
        CString &operator+=(const CString &c) { return append(c); }

        size_t size() const { return length; }
        const char *c_str() const { return data; }
    private:
        char *data;
        size_t length;
    };

    typedef Vector<char, 32> CStringBuffer;

    /**
    * @short Unicode string class
    */
    class UString {
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
        friend bool operator==(const UString&, const UString&);

    public:
        /**
        * @internal
        */
        struct Rep {

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
            static PassRefPtr<Rep> create(UChar *d, int l);
            static PassRefPtr<Rep> createCopying(const UChar *d, int l);
            static PassRefPtr<Rep> create(PassRefPtr<Rep> base, int offset, int length);

            // Constructs a string from a UTF-8 string, using strict conversion (see comments in UTF8.h).
            // Returns UString::Rep::null for null input or conversion failure.
            static PassRefPtr<Rep> createFromUTF8(const char*);

            void destroy();

            bool baseIsSelf() const { return baseString == this; }
            UChar* data() const { return baseString->buf + baseString->preCapacity + offset; }
            int size() const { return len; }

            unsigned hash() const { if (_hash == 0) _hash = computeHash(data(), len); return _hash; }
            unsigned computedHash() const { ASSERT(_hash); return _hash; } // fast path for Identifiers

            static unsigned computeHash(const UChar *, int length);
            static unsigned computeHash(const char *);

            Rep* ref() { ++rc; return this; }
            ALWAYS_INLINE void deref() { if (--rc == 0) destroy(); }

            // unshared data
            int offset;
            int len;
            int rc; // For null and empty static strings, this field does not reflect a correct count, because ref/deref are not thread-safe. A special case in destroy() guarantees that these do not get deleted.
            mutable unsigned _hash;
            IdentifierTable* identifierTable; // 0 if not an identifier. Since garbage collection can happen on a different thread, there is no other way to get to the table during destruction.
            UString::Rep* baseString;
            bool isStatic : 1;
            size_t reportedCost : 31;

            // potentially shared data
            UChar *buf;
            int usedCapacity;
            int capacity;
            int usedPreCapacity;
            int preCapacity;

            static Rep null;
            static Rep empty;
        };

    public:

        /**
        * Constructs a null string.
        */
        UString();
        /**
        * Constructs a string from a classical zero-terminated char string.
        */
        UString(const char *c);
        /**
        * Constructs a string from an array of Unicode characters of the specified
        * length.
        */
        UString(const UChar *c, int length);
        /**
        * If copy is false the string data will be adopted.
        * That means that the data will NOT be copied and the pointer will
        * be deleted when the UString object is modified or destroyed.
        * Behaviour defaults to a deep copy if copy is true.
        */
        UString(UChar *c, int length, bool copy);
        /**
        * Copy constructor. Makes a shallow copy only.
        */
        UString(const UString &s) : m_rep(s.m_rep) {}

        UString(const Vector<UChar>& buffer);

        /**
        * Convenience declaration only ! You'll be on your own to write the
        * implementation for a construction from DOM::DOMString.
        *
        * Note: feel free to contact me if you want to see a dummy header for
        * your favorite FooString class here !
        */
        UString(const DOM::DOMString&);
        /**
        * Convenience declaration only ! See UString(const DOM::DOMString&).
        */
        UString(const DOM::AtomicString&);

        /**
        * Concatenation constructor. Makes operator+ more efficient.
        */
        UString(const UString &, const UString &);
        /**
        * Destructor.
        */
        ~UString() {}

        // Special constructor for cases where we overwrite an object in place.
        UString(PlacementNewAdoptType) : m_rep(PlacementNewAdopt) { }

        /**
        * Constructs a string from an int.
        */
        static UString from(int i);
        /**
        * Constructs a string from an unsigned int.
        */
        static UString from(unsigned int u);
        /**
        * Constructs a string from a long int.
        */
        static UString from(long u);
        /**
        * Constructs a string from a double.
        */
        static UString from(double d);

        struct Range {
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
            Range(int pos, int len) : position(pos), length(len) {}
            Range() {}
            int position;
            int length;
        };

        UString spliceSubstringsWithSeparators(const Range* substringRanges, int rangeCount, const UString* separators, int separatorCount) const;

        /**
        * Append another string.
        */
        UString& append(const UString&);
        UString& append(const char*);
        UString& append(UChar);
        UString& append(char c) { return append(static_cast<UChar>(static_cast<unsigned char>(c))); }
        UString& append(const UChar*, int size);

        /**
        * @return The string converted to the 8-bit string type CString().
        * Returns false if any character is non-ASCII.
        */
        bool getCString(CStringBuffer&) const;

        /**
        * Convert the Unicode string to plain ASCII chars chopping off any higher
        * bytes. This method should only be used for *debugging* purposes as it
        * is neither Unicode safe nor free from side effects nor thread-safe.
        * In order not to waste any memory the char buffer is static and *shared*
        * by all UString instances.
        */
        char* ascii() const;

        /**
        * Convert the string to UTF-8, assuming it is UTF-16 encoded.
        * In non-strict mode, this function is tolerant of badly formed UTF-16, it
        * can create UTF-8 strings that are invalid because they have characters in
        * the range U+D800-U+DDFF, U+FFFE, or U+FFFF, but the UTF-8 string is
        * guaranteed to be otherwise valid.
        * In strict mode, error is returned as null CString.
        */
        CString UTF8String(bool strict = false) const;

        /**
        * @see UString(const DOM::DOMString&).
        */
        DOM::DOMString domString() const;

        /**
        * Assignment operator.
        */
        UString &operator=(const char *c);
        /**
        * Appends the specified string.
        */
        UString &operator+=(const UString &s) { return append(s); }
        UString &operator+=(const char *s) { return append(s); }

        /**
        * @return A pointer to the internal Unicode data.
        */
        const UChar* data() const { return m_rep->data(); }
        /**
        * @return True if null.
        */
        bool isNull() const { return (m_rep == &Rep::null); }
        /**
        * @return True if null or zero length.
        */
        bool isEmpty() const { return (!m_rep->len); }
        /**
        * Use this if you want to make sure that this string is a plain ASCII
        * string. For example, if you don't want to lose any information when
        * using cstring() or ascii().
        *
        * @return True if the string doesn't contain any non-ASCII characters.
        */
        bool is8Bit() const;
        /**
        * @return The length of the string.
        */
        int size() const { return m_rep->size(); }
        /**
        * Const character at specified position.
        */
        UChar operator[](int pos) const;

        /**
        * Attempts an conversion to a number. Apart from floating point numbers,
        * the algorithm will recognize hexadecimal representations (as
        * indicated by a 0x or 0X prefix) and +/- Infinity.
        * Returns NaN if the conversion failed.
        * @param tolerateTrailingJunk if true, toDouble can tolerate garbage after the number.
        * @param tolerateEmptyString if false, toDouble will turn an empty string into NaN rather than 0.
        */
        double toDouble(bool tolerateTrailingJunk, bool tolerateEmptyString) const;
        double toDouble(bool tolerateTrailingJunk) const;
        double toDouble() const;

        /**
        * Attempts an conversion to a 32-bit integer. ok will be set
        * according to the success.
        * @param tolerateEmptyString if false, toUInt32 will return false for *ok for an empty string.
        */
        uint32_t toUInt32(bool *ok = 0) const;
        uint32_t toUInt32(bool *ok, bool tolerateEmptyString) const;
        uint32_t toStrictUInt32(bool *ok = 0) const;

        /**
        * Attempts an conversion to an array index. The "ok" boolean will be set
        * to true if it is a valid array index according to the rule from
        * ECMA 15.2 about what an array index is. It must exactly match the string
        * form of an unsigned integer, and be less than 2^32 - 1.
        */
        unsigned toArrayIndex(bool *ok = 0) const;

        /**
        * @return Position of first occurrence of f starting at position pos.
        * -1 if the search was not successful.
        */
        int find(const UString &f, int pos = 0) const;
        int find(UChar, int pos = 0) const;
        /**
        * @return Position of first occurrence of f searching backwards from
        * position pos.
        * -1 if the search was not successful.
        */
        int rfind(const UString &f, int pos) const;
        int rfind(UChar, int pos) const;
        /**
        * @return The sub string starting at position pos and length len.
        */
        UString substr(int pos = 0, int len = -1) const;
        /**
        * Static instance of a null string.
        */
        static const UString &null();

        Rep* rep() const { return m_rep.get(); }
        UString(PassRefPtr<Rep> r) : m_rep(r) { ASSERT(m_rep); }

        size_t cost() const;

    private:
        size_t expandedSize(size_t size, size_t otherSize) const;
        int usedCapacity() const;
        int usedPreCapacity() const;
        void expandCapacity(int requiredLength);
        void expandPreCapacity(int requiredPreCap);

        RefPtr<Rep> m_rep;
    };

    bool operator==(const UString& s1, const UString& s2);
    inline bool operator!=(const UString& s1, const UString& s2) {
        return !KJS::operator==(s1, s2);
    }
    bool operator<(const UString& s1, const UString& s2);
    bool operator>(const UString& s1, const UString& s2);
    bool operator==(const UString& s1, const char *s2);
    inline bool operator!=(const UString& s1, const char *s2) {
        return !KJS::operator==(s1, s2);
    }
    inline bool operator==(const char *s1, const UString& s2) {
        return operator==(s2, s1);
    }
    inline bool operator!=(const char *s1, const UString& s2) {
        return !KJS::operator==(s1, s2);
    }
    bool operator==(const CString& s1, const CString& s2);
    inline UString operator+(const UString& s1, const UString& s2) {
        return UString(s1, s2);
    }

    int compare(const UString &, const UString &);

    bool equal(const UString::Rep*, const UString::Rep*);


    inline UString::UString()
        : m_rep(&Rep::null)
    {
    }

    // Rule from ECMA 15.2 about what an array index is.
    // Must exactly match string form of an unsigned integer, and be less than 2^32 - 1.
    inline unsigned UString::toArrayIndex(bool *ok) const
    {
        unsigned i = toStrictUInt32(ok);
        if (ok && i >= 0xFFFFFFFFU)
            *ok = false;
        return i;
    }

    // We'd rather not do shared substring append for small strings, since
    // this runs too much risk of a tiny initial string holding down a
    // huge buffer.
    // FIXME: this should be size_t but that would cause warnings until we
    // fix UString sizes to be size_t instead of int
    static const int minShareSize = Heap::minExtraCostSize / sizeof(UChar);

    inline size_t UString::cost() const
    {
        size_t capacity = (m_rep->baseString->capacity + m_rep->baseString->preCapacity) * sizeof(UChar);
        size_t reportedCost = m_rep->baseString->reportedCost;
        ASSERT(capacity >= reportedCost);

        size_t capacityDelta = capacity - reportedCost;

        if (capacityDelta < static_cast<size_t>(minShareSize))
            return 0;

#if COMPILER(MSVC)
        // MSVC complains about this assignment, since reportedCost is a 31-bit size_t.
#pragma warning(push)
#pragma warning(disable: 4267)
#endif

        m_rep->baseString->reportedCost = capacity;

#if COMPILER(MSVC)
#pragma warning(pop)
#endif

        return capacityDelta;
    }

} // namespace KJS


namespace WTF {

    template<typename T> struct DefaultHash;
    template<typename T> struct StrHash;

    template<> struct StrHash<KJS::UString::Rep*> {
        static unsigned hash(const KJS::UString::Rep* key) { return key->hash(); }
        static bool equal(const KJS::UString::Rep* a, const KJS::UString::Rep* b) { return KJS::equal(a, b); }
        static const bool safeToCompareToEmptyOrDeleted = false;
    };

    template<> struct StrHash<RefPtr<KJS::UString::Rep> > : public StrHash<KJS::UString::Rep*> {
        using StrHash<KJS::UString::Rep*>::hash;
        static unsigned hash(const RefPtr<KJS::UString::Rep>& key) { return key->hash(); }
        using StrHash<KJS::UString::Rep*>::equal;
        static bool equal(const RefPtr<KJS::UString::Rep>& a, const RefPtr<KJS::UString::Rep>& b) { return KJS::equal(a.get(), b.get()); }
        static bool equal(const KJS::UString::Rep* a, const RefPtr<KJS::UString::Rep>& b) { return KJS::equal(a, b.get()); }
        static bool equal(const RefPtr<KJS::UString::Rep>& a, const KJS::UString::Rep* b) { return KJS::equal(a.get(), b); }

        static const bool safeToCompareToEmptyOrDeleted = false;
    };

    template<> struct DefaultHash<KJS::UString::Rep*> {
        typedef StrHash<KJS::UString::Rep*> Hash;
    };

    template<> struct DefaultHash<RefPtr<KJS::UString::Rep> > {
        typedef StrHash<RefPtr<KJS::UString::Rep> > Hash;
    };
} // namespace WTF

#endif
