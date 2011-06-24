/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2005 Apple Computer, Inc.
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

#ifndef QualifiedName_h
#define QualifiedName_h

#include "AtomicString.h"

namespace WebCore {

class QualifiedName: public WTF::FastAllocBase {
public:
    class QualifiedNameImpl : public RefCounted<QualifiedNameImpl> {
    public:
        static PassRefPtr<QualifiedNameImpl> create(const AtomicString& p, const AtomicString& l, const AtomicString& n)
        {
            return adoptRef(new QualifiedNameImpl(p, l, n));
        }
        
        AtomicString m_prefix;
        AtomicString m_localName;
        AtomicString m_namespace;

    private:
        QualifiedNameImpl(const AtomicString& p, const AtomicString& l, const AtomicString& n)
            : m_prefix(p)
            , m_localName(l)
            , m_namespace(n)
        {
        }        
    };

    QualifiedName(const AtomicString& prefix, const AtomicString& localName, const AtomicString& namespaceURI);
    ~QualifiedName();
#ifdef QNAME_DEFAULT_CONSTRUCTOR
    QualifiedName() : m_impl(0) { }
#endif

    QualifiedName(const QualifiedName&);
    const QualifiedName& operator=(const QualifiedName&);

    bool operator==(const QualifiedName& other) const { return m_impl == other.m_impl; }
    bool operator!=(const QualifiedName& other) const { return !(*this == other); }

    bool matches(const QualifiedName& other) const { return m_impl == other.m_impl || (localName() == other.localName() && namespaceURI() == other.namespaceURI()); }

    bool hasPrefix() const { return m_impl->m_prefix != nullAtom; }
    void setPrefix(const AtomicString& prefix);

    const AtomicString& prefix() const { return m_impl->m_prefix; }
    const AtomicString& localName() const { return m_impl->m_localName; }
    const AtomicString& namespaceURI() const { return m_impl->m_namespace; }

    String toString() const;

    QualifiedNameImpl* impl() const { return m_impl; }
    
    // Init routine for globals
    static void init();

//+daw ca 23/07 global and static management
    static void unInit();

    static void staticFinalize();

private:
    static bool m_st_bInitialized;
public:
    void externalDeref();
//-daw ca

private:
    void ref() { m_impl->ref(); }
    void deref();
    
    QualifiedNameImpl* m_impl;
};

#ifndef WEBCORE_QUALIFIEDNAME_HIDE_GLOBALS
extern const QualifiedName anyName;
inline const QualifiedName& anyQName() { return anyName; }
#endif

inline bool operator==(const AtomicString& a, const QualifiedName& q) { return a == q.localName(); }
inline bool operator!=(const AtomicString& a, const QualifiedName& q) { return a != q.localName(); }
inline bool operator==(const QualifiedName& q, const AtomicString& a) { return a == q.localName(); }
inline bool operator!=(const QualifiedName& q, const AtomicString& a) { return a != q.localName(); }

}
#endif
