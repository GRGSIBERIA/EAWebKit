/* This file is part of the KDE project
   Copyright (C) 2000 Simon Hausmann <hausmann@kde.org>
   Copyright (C) 2006 Apple Computer, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

/*
* This file was modified by Electronic Arts Inc Copyright � 2009
*/

#ifndef MouseEventWithHitTestResults_h
#define MouseEventWithHitTestResults_h

#include <wtf/FastAllocBase.h>
#include "HitTestResult.h"
#include "PlatformMouseEvent.h"

namespace WebCore {

class PlatformScrollbar;

// FIXME: Why doesn't this class just cache a HitTestResult instead of copying all of HitTestResult's fields over?
class MouseEventWithHitTestResults: public WTF::FastAllocBase {
public:
    MouseEventWithHitTestResults(const PlatformMouseEvent&, const HitTestResult&);

    const PlatformMouseEvent& event() const { return m_event; }
    const HitTestResult& hitTestResult() const { return m_hitTestResult; }
    Node* targetNode() const;
    const IntPoint localPoint() const;
    PlatformScrollbar* scrollbar() const;
    bool isOverLink() const;

private:
    PlatformMouseEvent m_event;
    HitTestResult m_hitTestResult;
};

}

#endif
