/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSConsole.h"

#include "Console.h"

using namespace KJS;

namespace WebCore {

JSValue* JSConsole::debug(ExecState* exec, const ArgList& arguments)
{
    impl()->debug(exec, arguments);
    return jsUndefined();
}

JSValue* JSConsole::error(ExecState* exec, const ArgList& arguments)
{
    impl()->error(exec, arguments);
    return jsUndefined();
}

JSValue* JSConsole::info(ExecState* exec, const ArgList& arguments)
{
    impl()->info(exec, arguments);
    return jsUndefined();
}

JSValue* JSConsole::log(ExecState* exec, const ArgList& arguments)
{
    impl()->log(exec, arguments);
    return jsUndefined();
}

JSValue* JSConsole::warn(ExecState* exec, const ArgList& arguments)
{
    impl()->warn(exec, arguments);
    return jsUndefined();
}

JSValue* JSConsole::assertCondition(ExecState* exec, const ArgList& arguments)
{
    ArgList messageParameters;
    arguments.getSlice(1, messageParameters);

    impl()->assertCondition(arguments[0]->toBoolean(exec), exec, messageParameters);
    return jsUndefined();
}

JSValue* JSConsole::profile(ExecState* exec, const ArgList& arguments)
{
    impl()->profile(exec, arguments);
    return jsUndefined();
}

JSValue* JSConsole::profileEnd(ExecState* exec, const ArgList& arguments)
{
    impl()->profileEnd(exec, arguments);
    return jsUndefined();
}

} // namespace WebCore
