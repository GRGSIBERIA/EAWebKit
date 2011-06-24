/*
Copyright (C) 2008-2009 Electronic Arts, Inc.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1.  Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
2.  Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
3.  Neither the name of Electronic Arts, Inc. ("EA") nor the names of
    its contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY ELECTRONIC ARTS AND ITS CONTRIBUTORS "AS IS" AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL ELECTRONIC ARTS OR ITS CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

///////////////////////////////////////////////////////////////////////////////
// By Paul Pedriana - 2008
///////////////////////////////////////////////////////////////////////////////

#ifndef ContextMenu_h
#define ContextMenu_h

#include <wtf/FastAllocBase.h>
#include <wtf/Noncopyable.h>

#include "ContextMenuItem.h"
#include "HitTestResult.h"
#include "PlatformMenuDescription.h"
#include "PlatformString.h"

namespace WKAL {
class MenuEventProxy;

    class ContextMenuController;

    class ContextMenu : Noncopyable {
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
        ContextMenu(const HitTestResult&);
        ContextMenu(const HitTestResult&, const PlatformMenuDescription);
        ~ContextMenu();

        void populate();
        void addInspectElementItem();
        void checkOrEnableIfNeeded(ContextMenuItem&) const;

        void insertItem(unsigned position, ContextMenuItem&);
        void appendItem(ContextMenuItem&);
        
        ContextMenuItem* itemWithAction(unsigned);
        ContextMenuItem* itemAtIndex(unsigned, const PlatformMenuDescription);

        unsigned itemCount() const;

        HitTestResult hitTestResult() const { return m_hitTestResult; }
        ContextMenuController* controller() const;

        PlatformMenuDescription platformDescription() const;
        void setPlatformDescription(PlatformMenuDescription);

        PlatformMenuDescription releasePlatformDescription();

    private:
        HitTestResult m_hitTestResult;


        PlatformMenuDescription m_platformDescription;
    };

}

#endif // ContextMenu_h
