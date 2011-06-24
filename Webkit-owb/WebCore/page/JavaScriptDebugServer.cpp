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

#include "config.h"
#include "JavaScriptDebugServer.h"

#include "DOMWindow.h"
#include "EventLoop.h"
#include "Frame.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "JSDOMWindowCustom.h"
#include "JavaScriptCallFrame.h"
#include "JavaScriptDebugListener.h"
#include "Page.h"
#include "PageGroup.h"
#include "PluginView.h"
#include "ScrollView.h"
#include "Widget.h"
#include "ScriptController.h"
#include <kjs/DebuggerCallFrame.h>
#include <wtf/MainThread.h>
#include <wtf/UnusedParam.h>

using namespace KJS;

namespace WebCore {

typedef JavaScriptDebugServer::ListenerSet ListenerSet;

JavaScriptDebugServer& JavaScriptDebugServer::shared()
{
    static JavaScriptDebugServer server;
    return server;
}

JavaScriptDebugServer::JavaScriptDebugServer()
    : m_callingListeners(false)
    , m_pauseOnExceptions(false)
    , m_pauseOnNextStatement(false)
    , m_paused(false)
    , m_doneProcessingDebuggerEvents(true)
    , m_pauseOnCallFrame(0)
{
}

JavaScriptDebugServer::~JavaScriptDebugServer()
{
    deleteAllValues(m_pageListenersMap);
    deleteAllValues(m_breakpoints);
}

void JavaScriptDebugServer::addListener(JavaScriptDebugListener* listener)
{
    if (!hasListeners())
        Page::setDebuggerForAllPages(this);

    m_listeners.add(listener);
}

void JavaScriptDebugServer::removeListener(JavaScriptDebugListener* listener)
{
    m_listeners.remove(listener);
    if (!hasListeners()) {
        Page::setDebuggerForAllPages(0);
        m_doneProcessingDebuggerEvents = true;
    }
}

void JavaScriptDebugServer::addListener(JavaScriptDebugListener* listener, Page* page)
{
    ASSERT_ARG(page, page);

    if (!hasListeners())
        Page::setDebuggerForAllPages(this);

    pair<PageListenersMap::iterator, bool> result = m_pageListenersMap.add(page, 0);
    if (result.second)
        result.first->second = new ListenerSet;
    ListenerSet* listeners = result.first->second;

    listeners->add(listener);
}

void JavaScriptDebugServer::removeListener(JavaScriptDebugListener* listener, Page* page)
{
    ASSERT_ARG(page, page);

    PageListenersMap::iterator it = m_pageListenersMap.find(page);
    if (it == m_pageListenersMap.end())
        return;

    ListenerSet* listeners = it->second;

    listeners->remove(listener);

    if (listeners->isEmpty()) {
        m_pageListenersMap.remove(it);
        delete listeners;
    }

    if (!hasListeners()) {
        Page::setDebuggerForAllPages(0);
        m_doneProcessingDebuggerEvents = true;
    }
}

void JavaScriptDebugServer::pageCreated(Page* page)
{
    if (!hasListeners())
        return;

    page->setDebugger(this);
}

bool JavaScriptDebugServer::hasListenersInterestedInPage(Page* page)
{
    ASSERT_ARG(page, page);

    if (!m_listeners.isEmpty())
        return true;

    return m_pageListenersMap.contains(page);
}

void JavaScriptDebugServer::addBreakpoint(int sourceID, unsigned lineNumber)
{
    HashSet<unsigned>* lines = m_breakpoints.get(sourceID);
    if (!lines) {
        lines = new HashSet<unsigned>;
        m_breakpoints.set(sourceID, lines);
    }

    lines->add(lineNumber);
}

void JavaScriptDebugServer::removeBreakpoint(int sourceID, unsigned lineNumber)
{
    HashSet<unsigned>* lines = m_breakpoints.get(sourceID);
    if (!lines)
        return;

    lines->remove(lineNumber);

    if (!lines->isEmpty())
        return;

    m_breakpoints.remove(sourceID);
    delete lines;
}

bool JavaScriptDebugServer::hasBreakpoint(int sourceID, unsigned lineNumber) const
{
    HashSet<unsigned>* lines = m_breakpoints.get(sourceID);
    if (!lines)
        return false;
    return lines->contains(lineNumber);
}

void JavaScriptDebugServer::clearBreakpoints()
{
    deleteAllValues(m_breakpoints);
    m_breakpoints.clear();
}

void JavaScriptDebugServer::setPauseOnExceptions(bool pause)
{
    m_pauseOnExceptions = pause;
}

void JavaScriptDebugServer::pauseProgram()
{
    m_pauseOnNextStatement = true;
}

void JavaScriptDebugServer::continueProgram()
{
    if (!m_paused)
        return;

    m_pauseOnNextStatement = false;
    m_doneProcessingDebuggerEvents = true;
}

void JavaScriptDebugServer::stepIntoStatement()
{
    if (!m_paused)
        return;

    m_pauseOnNextStatement = true;
    m_doneProcessingDebuggerEvents = true;
}

void JavaScriptDebugServer::stepOverStatement()
{
    if (!m_paused)
        return;

    m_pauseOnCallFrame = m_currentCallFrame.get();
    m_doneProcessingDebuggerEvents = true;
}

void JavaScriptDebugServer::stepOutOfFunction()
{
    if (!m_paused)
        return;

    m_pauseOnCallFrame = m_currentCallFrame ? m_currentCallFrame->caller() : 0;
    m_doneProcessingDebuggerEvents = true;
}

JavaScriptCallFrame* JavaScriptDebugServer::currentCallFrame()
{
    if (!m_paused)
        return 0;
    return m_currentCallFrame.get();
}

static void dispatchDidParseSource(const ListenerSet& listeners, ExecState* exec, const KJS::SourceProvider& source, int startingLineNumber, const String& sourceURL, int sourceID)
{
    Vector<JavaScriptDebugListener*> copy;
    copyToVector(listeners, copy);
    for (size_t i = 0; i < copy.size(); ++i)
        copy[i]->didParseSource(exec, source, startingLineNumber, sourceURL, sourceID);
}

static void dispatchFailedToParseSource(const ListenerSet& listeners, ExecState* exec, const SourceProvider& source, int startingLineNumber, const String& sourceURL, int errorLine, const String& errorMessage)
{
    Vector<JavaScriptDebugListener*> copy;
    copyToVector(listeners, copy);
    for (size_t i = 0; i < copy.size(); ++i)
        copy[i]->failedToParseSource(exec, source, startingLineNumber, sourceURL, errorLine, errorMessage);
}

static Page* toPage(JSGlobalObject* globalObject)
{
    ASSERT_ARG(globalObject, globalObject);

    JSDOMWindow* window = asJSDOMWindow(globalObject);
    Frame* frame = window->impl()->frame();
    return frame ? frame->page() : 0;
}

void JavaScriptDebugServer::sourceParsed(ExecState* exec, int sourceID, const UString& sourceURL, const SourceProvider& source, int startingLineNumber, int errorLine, const UString& errorMessage)
{
    if (m_callingListeners)
        return;

    Page* page = toPage(exec->dynamicGlobalObject());
    if (!page)
        return;

    m_callingListeners = true;

    ASSERT(hasListeners());

    bool isError = errorLine != -1;

    if (!m_listeners.isEmpty()) {
        if (isError)
            dispatchFailedToParseSource(m_listeners, exec, source, startingLineNumber, sourceURL, errorLine, errorMessage);
        else
            dispatchDidParseSource(m_listeners, exec, source, startingLineNumber, sourceURL, sourceID);
    }

    if (ListenerSet* pageListeners = m_pageListenersMap.get(page)) {
        ASSERT(!pageListeners->isEmpty());
        if (isError)
            dispatchFailedToParseSource(*pageListeners, exec, source, startingLineNumber, sourceURL, errorLine, errorMessage);
        else
            dispatchDidParseSource(*pageListeners, exec, source, startingLineNumber, sourceURL, sourceID);
    }

    m_callingListeners = false;
}

static void dispatchFunctionToListeners(const ListenerSet& listeners, JavaScriptDebugServer::JavaScriptExecutionCallback callback)
{
    Vector<JavaScriptDebugListener*> copy;
    copyToVector(listeners, copy);
    for (size_t i = 0; i < copy.size(); ++i)
        (copy[i]->*callback)();
}

void JavaScriptDebugServer::dispatchFunctionToListeners(JavaScriptExecutionCallback callback, Page* page)
{
    if (m_callingListeners)
        return;

    m_callingListeners = true;

    ASSERT(hasListeners());

    WebCore::dispatchFunctionToListeners(m_listeners, callback);
    if (ListenerSet* pageListeners = m_pageListenersMap.get(page)) {
        ASSERT(!pageListeners->isEmpty());
        WebCore::dispatchFunctionToListeners(*pageListeners, callback);
    }

    m_callingListeners = false;
}

void JavaScriptDebugServer::setJavaScriptPaused(const PageGroup& pageGroup, bool paused)
{
    setMainThreadCallbacksPaused(paused);

    const HashSet<Page*>& pages = pageGroup.pages();

    HashSet<Page*>::const_iterator end = pages.end();
    for (HashSet<Page*>::const_iterator it = pages.begin(); it != end; ++it)
        setJavaScriptPaused(*it, false);
}

void JavaScriptDebugServer::setJavaScriptPaused(Page* page, bool paused)
{
    ASSERT_ARG(page, page);

    page->setDefersLoading(paused);

    for (Frame* frame = page->mainFrame(); frame; frame = frame->tree()->traverseNext())
        setJavaScriptPaused(frame, paused);
}

void JavaScriptDebugServer::setJavaScriptPaused(Frame* frame, bool paused)
{
    ASSERT_ARG(frame, frame);

    if (!frame->script()->isEnabled())
        return;

    frame->script()->setPaused(paused);

    if (JSDOMWindow* window = toJSDOMWindow(frame)) {
        if (paused)
            m_pausedTimeouts.set(frame, window->pauseTimeouts());
        else
            window->resumeTimeouts(m_pausedTimeouts.take(frame));
    }

    setJavaScriptPaused(frame->view(), paused);
}

void JavaScriptDebugServer::setJavaScriptPaused(FrameView* view, bool paused)
{
#if !PLATFORM(MAC)
    if (!view)
        return;

    HashSet<Widget*>* children = static_cast<ScrollView*>(view)->children();
    ASSERT(children);

    HashSet<Widget*>::iterator end = children->end();
    for (HashSet<Widget*>::iterator it = children->begin(); it != end; ++it) {
        Widget* widget = *it;
        if (!widget->isPluginView())
            continue;
        static_cast<PluginView*>(widget)->setJavaScriptPaused(paused);
    }
#endif
}

void JavaScriptDebugServer::pauseIfNeeded(Page* page)
{
    if (m_paused)
        return;

    if (!page || !hasListenersInterestedInPage(page))
        return;

    bool pauseNow = m_pauseOnNextStatement;
    pauseNow |= (m_pauseOnCallFrame == m_currentCallFrame);
    pauseNow |= (m_currentCallFrame->line() > 0 && hasBreakpoint(m_currentCallFrame->sourceIdentifier(), m_currentCallFrame->line()));
    if (!pauseNow)
        return;

    m_pauseOnCallFrame = 0;
    m_pauseOnNextStatement = false;
    m_paused = true;

    dispatchFunctionToListeners(&JavaScriptDebugListener::didPause, page);

    setJavaScriptPaused(page->group(), true);

    EventLoop loop;
    m_doneProcessingDebuggerEvents = false;
    while (!m_doneProcessingDebuggerEvents && !loop.ended())
        loop.cycle();

    setJavaScriptPaused(page->group(), false);

    m_paused = false;
}

void JavaScriptDebugServer::callEvent(const DebuggerCallFrame& debuggerCallFrame, int sourceID, int lineNumber)
{
    if (m_paused)
        return;

    m_currentCallFrame = JavaScriptCallFrame::create(debuggerCallFrame, m_currentCallFrame, sourceID, lineNumber);
    pauseIfNeeded(toPage(debuggerCallFrame.dynamicGlobalObject()));
}

void JavaScriptDebugServer::atStatement(const DebuggerCallFrame& debuggerCallFrame, int sourceID, int lineNumber)
{
    if (m_paused)
        return;

    ASSERT(m_currentCallFrame);
    if (!m_currentCallFrame)
        return;

    m_currentCallFrame->update(debuggerCallFrame, sourceID, lineNumber);
    pauseIfNeeded(toPage(debuggerCallFrame.dynamicGlobalObject()));
}

void JavaScriptDebugServer::returnEvent(const DebuggerCallFrame& debuggerCallFrame, int sourceID, int lineNumber)
{
    if (m_paused)
        return;

    ASSERT(m_currentCallFrame);
    if (!m_currentCallFrame)
        return;

    m_currentCallFrame->update(debuggerCallFrame, sourceID, lineNumber);
    pauseIfNeeded(toPage(debuggerCallFrame.dynamicGlobalObject()));

    // Treat stepping over a return statement like stepping out.
    if (m_currentCallFrame == m_pauseOnCallFrame)
        m_pauseOnCallFrame = m_currentCallFrame->caller();
    m_currentCallFrame = m_currentCallFrame->caller();
}

void JavaScriptDebugServer::exception(const DebuggerCallFrame& debuggerCallFrame, int sourceID, int lineNumber)
{
    if (m_paused)
        return;

    ASSERT(m_currentCallFrame);
    if (!m_currentCallFrame)
        return;

    if (m_pauseOnExceptions)
        m_pauseOnNextStatement = true;

    m_currentCallFrame->update(debuggerCallFrame, sourceID, lineNumber);
    pauseIfNeeded(toPage(debuggerCallFrame.dynamicGlobalObject()));
}

void JavaScriptDebugServer::willExecuteProgram(const DebuggerCallFrame& debuggerCallFrame, int sourceID, int lineNumber)
{
    if (m_paused)
        return;

    m_currentCallFrame = JavaScriptCallFrame::create(debuggerCallFrame, m_currentCallFrame, sourceID, lineNumber);
    pauseIfNeeded(toPage(debuggerCallFrame.dynamicGlobalObject()));
}

void JavaScriptDebugServer::didExecuteProgram(const DebuggerCallFrame& debuggerCallFrame, int sourceID, int lineNumber)
{
    if (m_paused)
        return;

    ASSERT(m_currentCallFrame);
    if (!m_currentCallFrame)
        return;

    m_currentCallFrame->update(debuggerCallFrame, sourceID, lineNumber);
    pauseIfNeeded(toPage(debuggerCallFrame.dynamicGlobalObject()));

    // Treat stepping over the end of a program like stepping out.
    if (m_currentCallFrame == m_pauseOnCallFrame)
        m_pauseOnCallFrame = m_currentCallFrame->caller();
    m_currentCallFrame = m_currentCallFrame->caller();
}

void JavaScriptDebugServer::didReachBreakpoint(const DebuggerCallFrame& debuggerCallFrame, int sourceID, int lineNumber)
{
    if (m_paused)
        return;
    
    ASSERT(m_currentCallFrame);
    if (!m_currentCallFrame)
        return;

    m_pauseOnNextStatement = true;
    m_currentCallFrame->update(debuggerCallFrame, sourceID, lineNumber);
    pauseIfNeeded(toPage(debuggerCallFrame.dynamicGlobalObject()));
}

} // namespace WebCore
