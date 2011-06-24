/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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
* This file was modified by Electronic Arts Inc Copyright � 2010
*/

#include "config.h"
#include "Console.h"

#include "ChromeClient.h"
#include "CString.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "InspectorController.h"
#include "Page.h"
#include "PageGroup.h"
#include "PlatformString.h"
#include <kjs/JSValue.h>
#include <kjs/interpreter.h>
#include <kjs/list.h>
#include <stdio.h>

using namespace KJS;

namespace WebCore {

Console::Console(Frame* frame)
    : m_frame(frame)
{
}

void Console::disconnectFrame()
{
    m_frame = 0;
}

static void printSourceURLAndLine(const String& sourceURL, unsigned lineNumber)
{
    if (!sourceURL.isEmpty()) {
        if (lineNumber > 0)
           OWB_PRINTF_FORMATTED("%s:%d: ", sourceURL.utf8().data(), lineNumber);
        else
           OWB_PRINTF_FORMATTED("%s: ", sourceURL.utf8().data());
    }
}

static void printMessageSourceAndLevelPrefix(MessageSource source, MessageLevel level)
{
    const char* sourceString;
    switch (source) {
        case HTMLMessageSource:
            sourceString = "HTML";
            break;
        case XMLMessageSource:
            sourceString = "XML";
            break;
        case JSMessageSource:
            sourceString = "JS";
            break;
        case CSSMessageSource:
            sourceString = "CSS";
            break;
        default:
            ASSERT_NOT_REACHED();
            // Fall thru.
        case OtherMessageSource:
            sourceString = "OTHER";
            break;
    }

    const char* levelString;
    switch (level) {
        case TipMessageLevel:
            levelString = "TIP";
            break;
        default:
            ASSERT_NOT_REACHED();
            // Fall thru.
        case LogMessageLevel:
            levelString = "LOG";
            break;
        case WarningMessageLevel:
            levelString = "WARN";
            break;
        case ErrorMessageLevel:
            levelString = "ERROR";
            break;
    }

   OWB_PRINTF_FORMATTED("%s %s:", sourceString, levelString);
}

static void printToStandardOut(MessageSource source, MessageLevel level, const String& message, const String& sourceURL, unsigned lineNumber)
{
    if (!Interpreter::shouldPrintExceptions())
        return;

    printSourceURLAndLine(sourceURL, lineNumber);
    printMessageSourceAndLevelPrefix(source, level);

   OWB_PRINTF_FORMATTED(" %s", message.utf8().data());
}

static void printToStandardOut(MessageLevel level, ExecState* exec, const ArgList& arguments, const KURL& url)
{
    if (!Interpreter::shouldPrintExceptions())
        return;

    printSourceURLAndLine(url.prettyURL(), 0);
    printMessageSourceAndLevelPrefix(JSMessageSource, level);

    for (size_t i = 0; i < arguments.size(); ++i) {
        UString argAsString = arguments[i]->toString(exec);
       OWB_PRINTF_FORMATTED(" %s", argAsString.UTF8String().c_str());
    }
}

void Console::addMessage(MessageSource source, MessageLevel level, const String& message, unsigned lineNumber, const String& sourceURL)
{
    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    if (source == JSMessageSource)
        page->chrome()->client()->addMessageToConsole(message, lineNumber, sourceURL);

    page->inspectorController()->addMessageToConsole(source, level, message, lineNumber, sourceURL);

    printToStandardOut(source, level, message, sourceURL, lineNumber);
}

void Console::debug(ExecState* exec, const ArgList& arguments)
{
    // In Firebug, console.debug has the same behavior as console.log. So we'll do the same.
    log(exec, arguments);
}

void Console::error(ExecState* exec, const ArgList& arguments)
{
    if (arguments.isEmpty())
        return;

    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    String message = arguments[0]->toString(exec);
    const KURL& url = m_frame->loader()->url();
    String prettyURL = url.prettyURL();

    page->chrome()->client()->addMessageToConsole(message, 0, prettyURL);
    page->inspectorController()->addMessageToConsole(JSMessageSource, ErrorMessageLevel, exec, arguments, 0, url.string());

    printToStandardOut(ErrorMessageLevel, exec, arguments, url);
}

void Console::info(ExecState* exec, const ArgList& arguments)
{
    if (arguments.isEmpty())
        return;

    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    String message = arguments[0]->toString(exec);
    const KURL& url = m_frame->loader()->url();
    String prettyURL = url.prettyURL();

    page->chrome()->client()->addMessageToConsole(message, 0, prettyURL);
    page->inspectorController()->addMessageToConsole(JSMessageSource, LogMessageLevel, exec, arguments, 0, url.string());

    printToStandardOut(LogMessageLevel, exec, arguments, url);
}

void Console::log(ExecState* exec, const ArgList& arguments)
{
    if (arguments.isEmpty())
        return;

    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    String message = arguments[0]->toString(exec);
    const KURL& url = m_frame->loader()->url();
    String prettyURL = url.prettyURL();

    page->chrome()->client()->addMessageToConsole(message, 0, prettyURL);
    page->inspectorController()->addMessageToConsole(JSMessageSource, LogMessageLevel, exec, arguments, 0, url.string());

    printToStandardOut(LogMessageLevel, exec, arguments, url);
}

void Console::assertCondition(bool condition, ExecState* exec, const ArgList& arguments)
{
    if (condition)
        return;

    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    const KURL& url = m_frame->loader()->url();

    // FIXME: <https://bugs.webkit.org/show_bug.cgi?id=19135> It would be nice to prefix assertion failures with a message like "Assertion failed: ".
    // FIXME: <https://bugs.webkit.org/show_bug.cgi?id=19136> We should print a message even when arguments.isEmpty() is true.

    page->inspectorController()->addMessageToConsole(JSMessageSource, ErrorMessageLevel, exec, arguments, 0, url.string());

    printToStandardOut(ErrorMessageLevel, exec, arguments, url);
}

void Console::profile(ExecState* exec, const ArgList& arguments)
{
    UString title = arguments[0]->toString(exec);
    Profiler::profiler()->startProfiling(exec, title, this);
}

void Console::profileEnd(ExecState* exec, const ArgList& arguments)
{
    UString title;
    if (arguments.size() >= 1)
        title = arguments[0]->toString(exec);

    Profiler::profiler()->stopProfiling(exec, title);
}

void Console::finishedProfiling(PassRefPtr<Profile> prpProfile)
{
    if (Page* page = m_frame->page())
        page->inspectorController()->addProfile(prpProfile);
}

void Console::warn(ExecState* exec, const ArgList& arguments)
{
    if (arguments.isEmpty())
        return;

    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    String message = arguments[0]->toString(exec);
    const KURL& url = m_frame->loader()->url();
    String prettyURL = url.prettyURL();

    page->chrome()->client()->addMessageToConsole(message, 0, prettyURL);
    page->inspectorController()->addMessageToConsole(JSMessageSource, WarningMessageLevel, exec, arguments, 0, url.string());

    printToStandardOut(WarningMessageLevel, exec, arguments, url);
}

} // namespace WebCore
