/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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

#include "config.h"
#include "JSCSSValue.h"

#include "CSSPrimitiveValue.h"
#include "CSSValueList.h"
#include "JSCSSPrimitiveValue.h"
#include "JSCSSValueList.h"

#if ENABLE(SVG)
#include "JSSVGColor.h"
#include "JSSVGPaint.h"
#include "SVGColor.h"
#include "SVGPaint.h"
#endif

using namespace KJS;

namespace WebCore {

JSValue* toJS(ExecState* exec, CSSValue* value)
{
    if (!value)
        return jsNull();

    DOMObject* ret = ScriptInterpreter::getDOMObject(value);

    if (ret)
        return ret;

    if (value->isValueList())
        ret = new (exec) JSCSSValueList(JSCSSValueListPrototype::self(exec), static_cast<CSSValueList*>(value));
#if ENABLE(SVG)
    else if (value->isSVGPaint())
        ret = new (exec) JSSVGPaint(JSSVGPaintPrototype::self(exec), static_cast<SVGPaint*>(value));
    else if (value->isSVGColor())
        ret = new (exec) JSSVGColor(JSSVGColorPrototype::self(exec), static_cast<SVGColor*>(value));
#endif
    else if (value->isPrimitiveValue())
        ret = new (exec) JSCSSPrimitiveValue(JSCSSPrimitiveValuePrototype::self(exec), static_cast<CSSPrimitiveValue*>(value));
    else
        ret = new (exec) JSCSSValue(JSCSSValuePrototype::self(exec), value);

    ScriptInterpreter::putDOMObject(value, ret);
    return ret;
}

} // namespace WebCore
