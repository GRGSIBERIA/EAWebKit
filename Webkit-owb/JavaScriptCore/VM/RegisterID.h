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

#ifndef RegisterID_h
#define RegisterID_h

#include <wtf/FastAllocBase.h>
#include <wtf/Assertions.h>
#include <wtf/Noncopyable.h>
#include <wtf/VectorTraits.h>

namespace KJS {

    class RegisterID : Noncopyable {
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
        RegisterID()
            : m_refCount(0)
#ifndef NDEBUG
            , m_didSetIndex(false)
#endif
        {
        }

        explicit RegisterID(int index)
            : m_refCount(0)
            , m_index(index)
#ifndef NDEBUG
            , m_didSetIndex(true)
#endif
        {
        }

        void setIndex(int index)
        {
            ASSERT(!m_refCount);
#ifndef NDEBUG
            m_didSetIndex = true;
#endif
            m_index = index;
        }

        int index() const
        {
            ASSERT(m_didSetIndex);
            return m_index;
        }

        bool isTemporary()
        {
            return m_index >= 0;
        }

        void ref()
        {
            ++m_refCount;
        }

        void deref()
        {
            --m_refCount;
            ASSERT(m_refCount >= 0);
        }

        int refCount() const
        {
            return m_refCount;
        }

    private:

        int m_refCount;
        int m_index;
#ifndef NDEBUG
        bool m_didSetIndex;
#endif
    };

    inline RegisterID* ignoredResult() { return reinterpret_cast<RegisterID*>(1); }

} // namespace KJS

namespace WTF {

    template<> struct VectorTraits<KJS::RegisterID> : VectorTraitsBase<true, KJS::RegisterID> {
        static const bool needsInitialization = true;
        static const bool canInitializeWithMemset = true; // Default initialization just sets everything to 0 or false, so this is safe.
    };

} // namespace WTF

#endif // RegisterID_h
