/*
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
* This file was modified by Electronic Arts Inc Copyright � 2009
*/

#ifndef GraphicsContextPrivate_h
#define GraphicsContextPrivate_h

#include <wtf/FastAllocBase.h>
#include "Font.h"

namespace WKAL {

    struct GraphicsContextState: public WTF::FastAllocBase {
        GraphicsContextState()
            : strokeStyle(SolidStroke)
            , strokeThickness(0)
            , strokeColor(Color::black)
            , fillColor(Color::black)
            , textDrawingMode(cTextFill)
            , paintingDisabled(false)
            , shadowBlur(0)
        {
        }

        Font font;
        StrokeStyle strokeStyle;
        float strokeThickness;
        Color strokeColor;
        Color fillColor;
        int textDrawingMode;
        bool paintingDisabled;
        IntSize shadowSize;
        unsigned shadowBlur;
        Color shadowColor;
    };

    class GraphicsContextPrivate: public WTF::FastAllocBase {
    public:
        GraphicsContextPrivate()
            : m_focusRingWidth(0)
            , m_focusRingOffset(0)
            , m_updatingControlTints(false)
        {
        }

        GraphicsContextState state;
        Vector<GraphicsContextState> stack;
        Vector<IntRect> m_focusRingRects;
        int m_focusRingWidth;
        int m_focusRingOffset;
        bool m_updatingControlTints;
    };

} // namespace WebCore

#endif // GraphicsContextPrivate_h
