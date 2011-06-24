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

#ifndef LabelID_h
#define LabelID_h

#include <wtf/FastAllocBase.h>
#include "CodeBlock.h"
#include "Instruction.h"
#include <wtf/Assertions.h>
#include <wtf/Vector.h>
#include <limits.h>

namespace KJS {

    class LabelID: public WTF::FastAllocBase {
    public:
        explicit LabelID(CodeBlock* codeBlock)
            : m_refCount(0)
            , m_location(invalidLocation)
            , m_codeBlock(codeBlock)
        {
        }

        // It doesn't really make sense to copy a LabelID, but we need this copy
        // constructor to support moving LabelIDs in a Vector.

        LabelID(const LabelID& other)
            : m_refCount(other.m_refCount)
            , m_location(other.m_location)
            , m_codeBlock(other.m_codeBlock)
            , m_unresolvedJumps(other.m_unresolvedJumps)
        {
        #ifndef NDEBUG
            const_cast<LabelID&>(other).m_codeBlock = 0;
        #endif
        }

        void setLocation(unsigned location)
        {
            ASSERT(m_codeBlock);
            m_location = location;

            unsigned size = m_unresolvedJumps.size();
            for (unsigned i = 0; i < size; ++i) {
                unsigned j = m_unresolvedJumps[i];
                m_codeBlock->instructions[j].u.operand = m_location - j;
            }
        }

        int offsetFrom(int location) const
        {
            ASSERT(m_codeBlock);
            if (m_location == invalidLocation) {
                m_unresolvedJumps.append(location);
                return 0;
            }
            return m_location - location;
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

        bool isForwardLabel() const { return m_location == invalidLocation; }

    private:
        typedef Vector<int, 8> JumpVector;

        static const unsigned invalidLocation = UINT_MAX;

        int m_refCount;
        unsigned m_location;
        CodeBlock* m_codeBlock;
        mutable JumpVector m_unresolvedJumps;
    };

} // namespace KJS

#endif // LabelID_h
