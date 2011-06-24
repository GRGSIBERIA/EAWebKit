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

#ifndef RegisterFile_h
#define RegisterFile_h

#include <wtf/FastAllocBase.h>
#include "Register.h"
#include "collector.h"
#if HAVE(MMAP)
#include <sys/mman.h>
#endif
#include <wtf/Noncopyable.h>

namespace KJS {

/*
    A register file is a stack of register frames. We represent a register
    frame by its offset from "base", the logical first entry in the register
    file. The bottom-most register frame's offset from base is 0.

    In a program where function "a" calls function "b" (global code -> a -> b),
    the register file might look like this:

    |       global frame     |        call frame      |        call frame      |     spare capacity     |
    -----------------------------------------------------------------------------------------------------
    |  0 |  1 |  2 |  3 |  4 |  5 |  6 |  7 |  8 |  9 | 10 | 11 | 12 | 13 | 14 |    |    |    |    |    | <-- index in buffer
    -----------------------------------------------------------------------------------------------------
    | -3 | -2 | -1 |  0 |  1 |  2 |  3 |  4 |  5 |  6 |  7 |  8 |  9 | 10 | 11 |    |    |    |    |    | <-- index relative to base
    -----------------------------------------------------------------------------------------------------
    |    <-globals | temps-> |  <-vars | temps->      |                 <-vars |
       ^              ^                   ^                                       ^
       |              |                   |                                       |
     buffer    base (frame 0)          frame 1                                 frame 2

    Since all variables, including globals, are accessed by negative offsets
    from their register frame pointers, to keep old global offsets correct, new
    globals must appear at the beginning of the register file, shifting base
    to the right.

    If we added one global variable to the register file depicted above, it
    would look like this:

    |         global frame        |<                                                                    >
    ------------------------------->                                                                    <
    |  0 |  1 |  2 |  3 |  4 |  5 |<                             >snip<                                 > <-- index in buffer
    ------------------------------->                                                                    <
    | -4 | -3 | -2 | -1 |  0 |  1 |<                                                                    > <-- index relative to base
    ------------------------------->                                                                    <
    |         <-globals | temps-> |
       ^                   ^
       |                   |
     buffer         base (frame 0)

    As you can see, global offsets relative to base have stayed constant,
    but base itself has moved. To keep up with possible changes to base,
    clients keep an indirect pointer, so their calculations update
    automatically when base changes.

    For client simplicity, the RegisterFile measures size and capacity from
    "base", not "buffer".
*/

    class JSGlobalObject;

    class RegisterFile : Noncopyable {
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
        enum {
            CallerCodeBlock = 0,
            ReturnVPC,
            CallerScopeChain,
            CallerRegisterOffset,
            ReturnValueRegister,
            ArgumentStartRegister,
            ArgumentCount,
            CalledAsConstructor,
            Callee,
            OptionalCalleeActivation,
            CallFrameHeaderSize
        };

        enum { ProgramCodeThisRegister = - 1 };

        enum { DefaultCapacity = 2 * 256 * 1024 / sizeof(Register)};   // Edited by Paul Pedriana. Perhaps we can reduce this further. Originally (2 * 1024 * 1024 / sizeof(Register))
        enum { DefaultMaxGlobals = 4 * 1024 };                         //                          Perhaps we can reduce this. Originally (8 * 1024)

        RegisterFile(size_t capacity = DefaultCapacity, size_t maxGlobals = DefaultMaxGlobals)
            : m_size(0)
            , m_capacity(capacity)
            , m_numGlobals(0)
            , m_maxGlobals(maxGlobals)
            , m_base(0)
            , m_buffer(0)
            , m_globalObject(0)
        {
            size_t bufferLength = (capacity + maxGlobals) * sizeof(Register);

            #if HAVE(MMAP)
                m_buffer = static_cast<Register*>(mmap(0, bufferLength, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0));
            #else
                // Note by Paul Pedriana: We may want to come up with a portable virtual memory interface that falls back to regular memory.
                m_buffer = static_cast<Register*>(fastMalloc(bufferLength));
            #endif

            m_base = m_buffer + maxGlobals;
        }

        ~RegisterFile();

        // Pointer to a value that holds the base of this register file.
        Register** basePointer() { return &m_base; }
        
        void setGlobalObject(JSGlobalObject* globalObject) { m_globalObject = globalObject; }
        JSGlobalObject* globalObject() { return m_globalObject; }

        void shrink(size_t size)
        {
            if (size < m_size)
                m_size = size;
        }

        bool grow(size_t size)
        {
            if (size > m_size) {
                if (size > m_capacity)
                    return false;
#if !HAVE(MMAP) && HAVE(VIRTUALALLOC)
                // FIXME: Use VirtualAlloc, and commit pages as we go.
#endif
                m_size = size;
            }
            return true;
        }

        size_t size() { return m_size; }
        
        void setNumGlobals(size_t numGlobals) { m_numGlobals = numGlobals; }
        int numGlobals() { return m_numGlobals; }
        size_t maxGlobals() { return m_maxGlobals; }

        Register* lastGlobal() { return m_base - m_numGlobals; }

        void mark(Heap* heap)
        {
            heap->markConservatively(lastGlobal(), m_base + m_size);
        }

    private:
        size_t m_size;
        size_t m_capacity;
        size_t m_numGlobals;
        size_t m_maxGlobals;
        Register* m_base;
        Register* m_buffer;
        JSGlobalObject* m_globalObject; // The global object whose vars are currently stored in the register file.
    };

} // namespace KJS

#endif // RegisterFile_h
