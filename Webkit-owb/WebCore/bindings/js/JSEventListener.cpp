/*
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All Rights Reserved.
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
#include "JSEventListener.h"

#include "CString.h"
#include "Console.h"
#include "DOMWindow.h"
#include "Document.h"
#include "Event.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "JSDOMWindow.h"
#include "JSEvent.h"
#include "JSEventTargetNode.h"
#include "ScriptController.h"
#include <kjs/FunctionConstructor.h>
#include <wtf/FastAllocBase.h>

using namespace KJS;

namespace WebCore {

void JSAbstractEventListener::handleEvent(Event* event, bool isWindowEvent)
{
    JSObject* listener = listenerObj();
    if (!listener)
        return;

    JSDOMWindow* window = this->window();
    // Null check as clearWindow() can clear this and we still get called back by
    // xmlhttprequest objects. See http://bugs.webkit.org/show_bug.cgi?id=13275
    if (!window)
        return;
    Frame* frame = window->impl()->frame();
    if (!frame)
        return;
    ScriptController* script = frame->script();
    if (!script->isEnabled() || script->isPaused())
        return;

    JSLock lock;

    ExecState* exec = window->globalExec();

    JSValue* handleEventFunction = listener->get(exec, Identifier(exec, "handleEvent"));
    CallData callData;
    CallType callType = handleEventFunction->getCallData(callData);
    if (callType == CallTypeNone) {
        handleEventFunction = 0;
        callType = listener->getCallData(callData);
    }

    if (callType != CallTypeNone) {
        ref();

        ArgList args;
        args.append(toJS(exec, event));

        Event* savedEvent = window->currentEvent();
        window->setCurrentEvent(event);

        JSValue* retval;
        if (handleEventFunction) {
            window->startTimeoutCheck();
            retval = call(exec, handleEventFunction, callType, callData, listener, args);
        } else {
            JSValue* thisValue;
            if (isWindowEvent)
                thisValue = window->shell();
            else
                thisValue = toJS(exec, event->currentTarget());
            window->startTimeoutCheck();
            retval = call(exec, listener, callType, callData, thisValue, args);
        }
        window->stopTimeoutCheck();

        window->setCurrentEvent(savedEvent);

        if (exec->hadException()) {
            JSObject* exception = exec->exception()->toObject(exec);
            String message = exception->get(exec, exec->propertyNames().message)->toString(exec);
            int lineNumber = exception->get(exec, Identifier(exec, "line"))->toInt32(exec);
            String sourceURL = exception->get(exec, Identifier(exec, "sourceURL"))->toString(exec);
            frame->domWindow()->console()->addMessage(JSMessageSource, ErrorMessageLevel, message, lineNumber, sourceURL);
            exec->clearException();
        } else {
            if (!retval->isUndefinedOrNull() && event->storesResultAsString())
                event->storeResult(retval->toString(exec));
            if (m_isHTML) {
                bool retvalbool;
                if (retval->getBoolean(retvalbool) && !retvalbool)
                    event->preventDefault();
            }
        }

        Document::updateDocumentsRendering();
        deref();
    }
}

bool JSAbstractEventListener::isHTMLEventListener() const
{
    return m_isHTML;
}

// -------------------------------------------------------------------------

JSUnprotectedEventListener::JSUnprotectedEventListener(JSObject* listener, JSDOMWindow* window, bool isHTML)
    : JSAbstractEventListener(isHTML)
    , m_listener(listener)
    , m_window(window)
{
    if (m_listener) {
        JSDOMWindow::UnprotectedListenersMap& listeners = isHTML
            ? window->jsUnprotectedHTMLEventListeners() : window->jsUnprotectedEventListeners();
        listeners.set(m_listener, this);
    }
}

JSUnprotectedEventListener::~JSUnprotectedEventListener()
{
    if (m_listener && m_window) {
        JSDOMWindow::UnprotectedListenersMap& listeners = isHTMLEventListener()
            ? m_window->jsUnprotectedHTMLEventListeners() : m_window->jsUnprotectedEventListeners();
        listeners.remove(m_listener);
    }
}

JSObject* JSUnprotectedEventListener::listenerObj() const
{
    return m_listener;
}

JSDOMWindow* JSUnprotectedEventListener::window() const
{
    return m_window;
}

void JSUnprotectedEventListener::clearWindow()
{
    m_window = 0;
}

void JSUnprotectedEventListener::mark()
{
    if (m_listener && !m_listener->marked())
        m_listener->mark();
}

#ifndef NDEBUG
#ifndef LOG_CHANNEL_PREFIX
#define LOG_CHANNEL_PREFIX Log
#endif
WTFLogChannel LogWebCoreEventListenerLeaks = { 0x00000000, "", WTFLogChannelOn };

struct EventListenerCounter: public WTF::FastAllocBase {
    static unsigned count;
    ~EventListenerCounter()
    {
        if (count)
            LOG(WebCoreEventListenerLeaks, "LEAK: %u EventListeners\n", count);
    }
};
unsigned EventListenerCounter::count = 0;
static EventListenerCounter eventListenerCounter;
#endif

// -------------------------------------------------------------------------

JSEventListener::JSEventListener(JSObject* listener, JSDOMWindow* window, bool isHTML)
    : JSAbstractEventListener(isHTML)
    , m_listener(listener)
    , m_window(window)
{
    if (m_listener) {
        JSDOMWindow::ListenersMap& listeners = isHTML
            ? m_window->jsHTMLEventListeners() : m_window->jsEventListeners();
        listeners.set(m_listener, this);
    }
#ifndef NDEBUG
    ++eventListenerCounter.count;
#endif
}

JSEventListener::~JSEventListener()
{
    if (m_listener && m_window) {
        JSDOMWindow::ListenersMap& listeners = isHTMLEventListener()
            ? m_window->jsHTMLEventListeners() : m_window->jsEventListeners();
        listeners.remove(m_listener);
    }
#ifndef NDEBUG
    --eventListenerCounter.count;
#endif
}

JSObject* JSEventListener::listenerObj() const
{
    return m_listener;
}

JSDOMWindow* JSEventListener::window() const
{
    return m_window;
}

void JSEventListener::clearWindow()
{
    m_window = 0;
}

// -------------------------------------------------------------------------

JSLazyEventListener::JSLazyEventListener(const String& functionName, const String& code, JSDOMWindow* window, Node* node, int lineNumber)
    : JSEventListener(0, window, true)
    , m_functionName(functionName)
    , m_code(code)
    , m_parsed(false)
    , m_lineNumber(lineNumber)
    , m_originalNode(node)
{
    // We don't retain the original node because we assume it
    // will stay alive as long as this handler object is around
    // and we need to avoid a reference cycle. If JS transfers
    // this handler to another node, parseCode will be called and
    // then originalNode is no longer needed.

    // A JSLazyEventListener can be created with a line number of zero when it is created with
    // a setAttribute call from JavaScript, so make the line number 1 in that case.
    if (m_lineNumber == 0)
        m_lineNumber = 1;
}

JSObject* JSLazyEventListener::listenerObj() const
{
    parseCode();
    return m_listener;
}


///////////////////////
// Note by Paul Pedriana (1/2009): This code is problematic because it declares a ProtectedPtr 
// as a static variable, which will be destructed on app shutdown. This causes it to execute 
// code which is unsafe and results in a crash since we have shut down the system that it 
// is relying on. It looks like the WebKit trunk at webkit.org has removed this code, 
// but we have to deal with it in our version for the time being.
//
// JSValue* JSLazyEventListener::eventParameterName() const
// {
//     static ProtectedPtr<JSValue> eventString = jsString(window()->globalExec(), "event");
//     return eventString.get();
// }

// My current workaround:
static ProtectedPtr<JSValue>* gpEventString;

JSValue* JSLazyEventListener::eventParameterName() const
{
    if(!gpEventString)
    {
        KJS::JSString* pString = jsString(window()->globalExec(), "event");
        gpEventString = new ProtectedPtr<JSValue>(pString);
    }
    return gpEventString->get();
}

void JSLazyEventListener::staticFinalize()
{
    delete gpEventString; // This will do JSValue shutdown processing.
    gpEventString = NULL;
}
///////////////////////


void JSLazyEventListener::parseCode() const
{
    if (m_parsed)
        return;
    m_parsed = true;

    Frame* frame = window()->impl()->frame();
    if (frame && frame->script()->isEnabled()) {
        ExecState* exec = window()->globalExec();

        JSLock lock;
        ArgList args;

        UString sourceURL(frame->loader()->url().string());
        args.append(eventParameterName());
        args.append(jsString(exec, m_code));

        // FIXME: Passing the document's URL to construct is not always correct, since this event listener might
        // have been added with setAttribute from a script, and we should pass String() in that case.
        m_listener = constructFunction(exec, args, Identifier(exec, m_functionName), sourceURL, m_lineNumber); // FIXME: is globalExec ok?

        JSFunction* listenerAsFunction = static_cast<JSFunction*>(m_listener.get());

        if (exec->hadException()) {
            exec->clearException();

            // failed to parse, so let's just make this listener a no-op
            m_listener = 0;
        } else if (m_originalNode) {
            // Add the event's home element to the scope
            // (and the document, and the form - see JSHTMLElement::eventHandlerScope)
            ScopeChain scope = listenerAsFunction->scope();

            JSValue* thisObj = toJS(exec, m_originalNode);
            if (thisObj->isObject()) {
                static_cast<JSEventTargetNode*>(thisObj)->pushEventHandlerScope(exec, scope);
                listenerAsFunction->setScope(scope);
            }
        }
    }

    // no more need to keep the unparsed code around
    m_functionName = String();
    m_code = String();

    if (m_listener) {
        JSDOMWindow::ListenersMap& listeners = isHTMLEventListener()
            ? window()->jsHTMLEventListeners() : window()->jsEventListeners();
        listeners.set(m_listener, const_cast<JSLazyEventListener*>(this));
    }
}

JSValue* getNodeEventListener(EventTargetNode* n, const AtomicString& eventType)
{
    if (JSAbstractEventListener* listener = static_cast<JSAbstractEventListener*>(n->getHTMLEventListener(eventType))) {
        if (JSValue* obj = listener->listenerObj())
            return obj;
    }
    return jsNull();
}

} // namespace WebCore
