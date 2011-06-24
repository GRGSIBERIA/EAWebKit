/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*
* This file was modified by Electronic Arts Inc Copyright � 2009
*/

#include "config.h"
#include "ScriptController.h"

#include "Console.h"
#include "DOMWindow.h"
#include "Document.h"
#include "Event.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "GCController.h"
#include "JSDocument.h"
#include "JSDOMWindow.h"
#include "Page.h"
#include "PageGroup.h"
#include "Settings.h"
#include "StringSourceProvider.h"
#include "JSEventListener.h"
#include <kjs/completion.h>
#include <kjs/debugger.h>
#include <EAWebKit/EAWebKitView.h>  // For user notify
#if ENABLE(SVG)
#include "JSSVGLazyEventListener.h"
#endif

using namespace KJS;

namespace WebCore {

ScriptController::ScriptController(Frame* frame)
    : m_frame(frame)
    , m_handlerLineno(0)
    , m_processingTimerCallback(0)
    , m_processingInlineCode(0)
    , m_paused(false)
{
}

ScriptController::~ScriptController()
{
    if (m_windowShell) {
        m_windowShell = 0;
    
        // It's likely that releasing the global object has created a lot of garbage.
        gcController().garbageCollectSoon();
    }
}

JSValue* ScriptController::evaluate(const String& filename, int baseLine, const String& str) 
{
    // 11/09/09 CSidhall - Added notify process start to user 
	NOTIFY_PROCESS_STATUS(EA::WebKit::kVProcessTypeScript, EA::WebKit::kVProcessStatusStarted);
	    
    // evaluate code. Returns the JS return value or 0
    // if there was none, an error occured or the type couldn't be converted.

    initScriptIfNeeded();
    // inlineCode is true for <a href="javascript:doSomething()">
    // and false for <script>doSomething()</script>. Check if it has the
    // expected value in all cases.
    // See smart window.open policy for where this is used.
    ExecState* exec = m_windowShell->window()->globalExec();
    m_processingInlineCode = filename.isNull();

    JSLock lock;

    // Evaluating the JavaScript could cause the frame to be deallocated
    // so we start the keep alive timer here.
    m_frame->keepAlive();

    m_windowShell->window()->startTimeoutCheck();
    Completion comp = Interpreter::evaluate(exec, exec->dynamicGlobalObject()->globalScopeChain(), filename, baseLine, StringSourceProvider::create(str), m_windowShell);
    m_windowShell->window()->stopTimeoutCheck();

	NOTIFY_PROCESS_STATUS(EA::WebKit::kVProcessTypeScript, EA::WebKit::kVProcessStatusEnded);

    if (comp.complType() == Normal || comp.complType() == ReturnValue) {
        m_processingInlineCode = false;
        return comp.value();
    }

    if (comp.complType() == Throw) {
        UString errorMessage = comp.value()->toString(exec);
        int lineNumber = comp.value()->toObject(exec)->get(exec, Identifier(exec, "line"))->toInt32(exec);
        UString sourceURL = comp.value()->toObject(exec)->get(exec, Identifier(exec, "sourceURL"))->toString(exec);
        m_frame->domWindow()->console()->addMessage(JSMessageSource, ErrorMessageLevel, errorMessage, lineNumber, sourceURL);
    }

    m_processingInlineCode = false;
    return 0;
}

void ScriptController::clear()
{
    if (!m_windowShell)
        return;

    JSLock lock;
    m_windowShell->window()->clear();
    m_liveFormerWindows.add(m_windowShell->window());
    m_windowShell->setWindow(new JSDOMWindow(m_frame->domWindow(), m_windowShell));
    if (Page* page = m_frame->page()) {
        attachDebugger(page->debugger());
        m_windowShell->window()->setPageGroupIdentifier(page->group().identifier());
    }

    // There is likely to be a lot of garbage now.
    gcController().garbageCollectSoon();
}

PassRefPtr<EventListener> ScriptController::createHTMLEventHandler(const String& functionName, const String& code, Node* node)
{
    initScriptIfNeeded();
    JSLock lock;
    return JSLazyEventListener::create(functionName, code, m_windowShell->window(), node, m_handlerLineno);
}

#if ENABLE(SVG)
PassRefPtr<EventListener> ScriptController::createSVGEventHandler(const String& functionName, const String& code, Node* node)
{
    initScriptIfNeeded();
    JSLock lock;
    return JSSVGLazyEventListener::create(functionName, code, m_windowShell->window(), node, m_handlerLineno);
}
#endif

void ScriptController::finishedWithEvent(Event* event)
{
  // This is called when the DOM implementation has finished with a particular event. This
  // is the case in sitations where an event has been created just for temporary usage,
  // e.g. an image load or mouse move. Once the event has been dispatched, it is forgotten
  // by the DOM implementation and so does not need to be cached still by the interpreter
  ScriptInterpreter::forgetDOMObject(event);
}

void ScriptController::initScript()
{
    if (m_windowShell)
        return;

    JSLock lock;

    m_windowShell = new JSDOMWindowShell(m_frame->domWindow());
    updateDocument();

    if (Page* page = m_frame->page()) {
        attachDebugger(page->debugger());
        m_windowShell->window()->setPageGroupIdentifier(page->group().identifier());
    }

    m_frame->loader()->dispatchWindowObjectAvailable();
}

bool ScriptController::processingUserGesture() const
{
    if (!m_windowShell)
        return false;

    if (Event* event = m_windowShell->window()->currentEvent()) {
        const AtomicString& type = event->type();
        const WebCore::EventNames& evtNames = WebCore::eventNames();
        if ( // mouse events
            type == evtNames.clickEvent || type == evtNames.mousedownEvent ||
            type == evtNames.mouseupEvent || type == evtNames.dblclickEvent ||
            // keyboard events
            type == evtNames.keydownEvent || type == evtNames.keypressEvent ||
            type == evtNames.keyupEvent ||
            // other accepted events
            type == evtNames.selectEvent || type == evtNames.changeEvent ||
            type == evtNames.focusEvent || type == evtNames.blurEvent ||
            type == evtNames.submitEvent)
            return true;
    } else { // no event
        if (m_processingInlineCode && !m_processingTimerCallback) {
            // This is the <a href="javascript:window.open('...')> case -> we let it through
            return true;
        }
        // This is the <script>window.open(...)</script> case or a timer callback -> block it
    }
    return false;
}

bool ScriptController::isEnabled()
{
    Settings* settings = m_frame->settings();
    return (settings && settings->isJavaScriptEnabled());
}

void ScriptController::attachDebugger(KJS::Debugger* debugger)
{
    if (!m_windowShell)
        return;

    if (debugger)
        debugger->attach(m_windowShell->window());
    else if (KJS::Debugger* currentDebugger = m_windowShell->window()->debugger())
        currentDebugger->detach(m_windowShell->window());
}

void ScriptController::updateDocument()
{
    if (!m_frame->document())
        return;

    JSLock lock;
    if (m_windowShell)
        m_windowShell->window()->updateDocument();
    HashSet<JSDOMWindow*>::iterator end = m_liveFormerWindows.end();
    for (HashSet<JSDOMWindow*>::iterator it = m_liveFormerWindows.begin(); it != end; ++it)
        (*it)->updateDocument();
}


} // namespace WebCore
