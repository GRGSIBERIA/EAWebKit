// -*- mode: c++; c-basic-offset: 4 -*-
/*
 *  Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
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

#ifndef PropertyNameArray_h
#define PropertyNameArray_h

#include <wtf/FastAllocBase.h>
#include "ExecState.h"
#include "identifier.h"
#include <wtf/HashSet.h>
#include <wtf/Vector.h>

namespace KJS {

    class PropertyNameArray {
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
        typedef Identifier ValueType;
        typedef Vector<Identifier>::const_iterator const_iterator;

        PropertyNameArray(JSGlobalData* globalData) : m_globalData(globalData) {}
        PropertyNameArray(ExecState* exec) : m_globalData(&exec->globalData()) {}

        void add(const Identifier& identifier) { add(identifier.ustring().rep()); }
        void add(UString::Rep*);
        void addKnownUnique(UString::Rep* identifier) { m_vector.append(Identifier(m_globalData, identifier)); }

        const_iterator begin() const { return m_vector.begin(); }
        const_iterator end() const { return m_vector.end(); }

        size_t size() const { return m_vector.size(); }

        Identifier& operator[](unsigned i) { return m_vector[i]; }
        const Identifier& operator[](unsigned i) const { return m_vector[i]; }

        Identifier* releaseIdentifiers() { return size() ? m_vector.releaseBuffer() : 0; }

    private:
        typedef HashSet<UString::Rep*, PtrHash<UString::Rep*> > IdentifierSet;

        Vector<Identifier, 20> m_vector;
        IdentifierSet m_set;
        JSGlobalData* m_globalData;
    };

} // namespace KJS

#endif // PropertyNameArray_h
