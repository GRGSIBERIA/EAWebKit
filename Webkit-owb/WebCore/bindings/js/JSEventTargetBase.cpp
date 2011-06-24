/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *           (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "config.h"
#include "JSEventTargetBase.h"

#include "JSDOMWindow.h"
#include "JSEventTargetNode.h"
#include "JSEventListener.h"
#include "EventNames.h"

#if ENABLE(SVG)
#include "JSSVGElementInstance.h"
#endif

using namespace KJS;

namespace WebCore {

static JSValue* jsEventTargetAddEventListener(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* jsEventTargetRemoveEventListener(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* jsEventTargetDispatchEvent(ExecState*, JSObject*, JSValue*, const ArgList&);

}

#include "JSEventTargetBase.lut.h"

namespace WebCore {

template<> Identifier*  JSEventTargetPrototype<JSNodePrototype,JSEventTargetPrototypeInformation>::m_pPrototypeName=NULL;  // 3/26/09 CSidhall - Added to get access for leak fix
    
/* Source for JSEventTargetPropertiesTable
@begin JSEventTargetPropertiesTable
onabort       WebCore::JSEventTargetProperties::OnAbort                DontDelete
onblur        WebCore::JSEventTargetProperties::OnBlur                 DontDelete
onchange      WebCore::JSEventTargetProperties::OnChange               DontDelete
onclick       WebCore::JSEventTargetProperties::OnClick                DontDelete
oncontextmenu WebCore::JSEventTargetProperties::OnContextMenu          DontDelete
ondblclick    WebCore::JSEventTargetProperties::OnDblClick             DontDelete
onbeforecut   WebCore::JSEventTargetProperties::OnBeforeCut            DontDelete
oncut         WebCore::JSEventTargetProperties::OnCut                  DontDelete
onbeforecopy  WebCore::JSEventTargetProperties::OnBeforeCopy           DontDelete
oncopy        WebCore::JSEventTargetProperties::OnCopy                 DontDelete
onbeforepaste WebCore::JSEventTargetProperties::OnBeforePaste          DontDelete
onpaste       WebCore::JSEventTargetProperties::OnPaste                DontDelete
ondrag        WebCore::JSEventTargetProperties::OnDrag                 DontDelete
ondragend     WebCore::JSEventTargetProperties::OnDragEnd              DontDelete
ondragenter   WebCore::JSEventTargetProperties::OnDragEnter            DontDelete
ondragleave   WebCore::JSEventTargetProperties::OnDragLeave            DontDelete
ondragover    WebCore::JSEventTargetProperties::OnDragOver             DontDelete
ondragstart   WebCore::JSEventTargetProperties::OnDragStart            DontDelete
ondrop        WebCore::JSEventTargetProperties::OnDrop                 DontDelete
onerror       WebCore::JSEventTargetProperties::OnError                DontDelete
onfocus       WebCore::JSEventTargetProperties::OnFocus                DontDelete
oninput       WebCore::JSEventTargetProperties::OnInput                DontDelete
onkeydown     WebCore::JSEventTargetProperties::OnKeyDown              DontDelete
onkeypress    WebCore::JSEventTargetProperties::OnKeyPress             DontDelete
onkeyup       WebCore::JSEventTargetProperties::OnKeyUp                DontDelete
onload        WebCore::JSEventTargetProperties::OnLoad                 DontDelete
onmousedown   WebCore::JSEventTargetProperties::OnMouseDown            DontDelete
onmousemove   WebCore::JSEventTargetProperties::OnMouseMove            DontDelete
onmouseout    WebCore::JSEventTargetProperties::OnMouseOut             DontDelete
onmouseover   WebCore::JSEventTargetProperties::OnMouseOver            DontDelete
onmouseup     WebCore::JSEventTargetProperties::OnMouseUp              DontDelete
onmousewheel  WebCore::JSEventTargetProperties::OnMouseWheel           DontDelete
onreset       WebCore::JSEventTargetProperties::OnReset                DontDelete
onresize      WebCore::JSEventTargetProperties::OnResize               DontDelete
onscroll      WebCore::JSEventTargetProperties::OnScroll               DontDelete
onsearch      WebCore::JSEventTargetProperties::OnSearch               DontDelete
onselect      WebCore::JSEventTargetProperties::OnSelect               DontDelete
onselectstart WebCore::JSEventTargetProperties::OnSelectStart          DontDelete
onsubmit      WebCore::JSEventTargetProperties::OnSubmit               DontDelete
onunload      WebCore::JSEventTargetProperties::OnUnload               DontDelete
@end
*/

/*
@begin JSEventTargetPrototypeTable
addEventListener        WebCore::jsEventTargetAddEventListener    DontDelete|Function 3
removeEventListener     WebCore::jsEventTargetRemoveEventListener DontDelete|Function 3
dispatchEvent           WebCore::jsEventTargetDispatchEvent       DontDelete|Function 1
@end
*/

static bool retrieveEventTargetAndCorrespondingNode(ExecState*, JSValue* thisValue, Node*& eventNode, EventTarget*& eventTarget)
{
    if (!thisValue->isObject(&JSNode::s_info))
        return false;

    JSEventTargetNode* jsNode = static_cast<JSEventTargetNode*>(thisValue);
    ASSERT(jsNode);

    EventTargetNode* node = static_cast<EventTargetNode*>(jsNode->impl());
    ASSERT(node);

    eventNode = node;
    eventTarget = node;
    return true;
}

JSValue* jsEventTargetAddEventListener(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    Node* eventNode = 0;
    EventTarget* eventTarget = 0;
    if (!retrieveEventTargetAndCorrespondingNode(exec, thisValue, eventNode, eventTarget))
        return throwError(exec, TypeError);

    Frame* frame = eventNode->document()->frame();
    if (!frame)
        return jsUndefined();

    if (RefPtr<JSEventListener> listener = toJSDOMWindow(frame)->findOrCreateJSEventListener(exec, args[1]))
        eventTarget->addEventListener(args[0]->toString(exec), listener.release(), args[2]->toBoolean(exec));

    return jsUndefined();
}

JSValue* jsEventTargetRemoveEventListener(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    Node* eventNode = 0;
    EventTarget* eventTarget = 0;
    if (!retrieveEventTargetAndCorrespondingNode(exec, thisValue, eventNode, eventTarget))
        return throwError(exec, TypeError);

    Frame* frame = eventNode->document()->frame();
    if (!frame)
        return jsUndefined();

    if (JSEventListener* listener = toJSDOMWindow(frame)->findJSEventListener(args[1]))
        eventTarget->removeEventListener(args[0]->toString(exec), listener, args[2]->toBoolean(exec));

    return jsUndefined();
}

JSValue* jsEventTargetDispatchEvent(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    Node* eventNode = 0;
    EventTarget* eventTarget = 0;
    if (!retrieveEventTargetAndCorrespondingNode(exec, thisValue, eventNode, eventTarget))
        return throwError(exec, TypeError);

    DOMExceptionTranslator exception(exec);
    return jsBoolean(eventTarget->dispatchEvent(toEvent(args[0]), exception));
}

const AtomicString& eventNameForPropertyToken(int token)
{    
    const WebCore::EventNames& evtNames = WebCore::eventNames();
    switch (token) {
    case JSEventTargetProperties::OnAbort:
        return evtNames.abortEvent;
    case JSEventTargetProperties::OnBlur:
        return evtNames.blurEvent;
    case JSEventTargetProperties::OnChange:
        return evtNames.changeEvent;
    case JSEventTargetProperties::OnClick:
        return evtNames.clickEvent;
    case JSEventTargetProperties::OnContextMenu:
        return evtNames.contextmenuEvent;
    case JSEventTargetProperties::OnDblClick:
        return evtNames.dblclickEvent;
    case JSEventTargetProperties::OnError:
        return evtNames.errorEvent;
    case JSEventTargetProperties::OnFocus:
        return evtNames.focusEvent;
    case JSEventTargetProperties::OnInput:
        return evtNames.inputEvent;
    case JSEventTargetProperties::OnKeyDown:
        return evtNames.keydownEvent;
    case JSEventTargetProperties::OnKeyPress:
        return evtNames.keypressEvent;
    case JSEventTargetProperties::OnKeyUp:
        return evtNames.keyupEvent;
    case JSEventTargetProperties::OnLoad:
        return evtNames.loadEvent;
    case JSEventTargetProperties::OnMouseDown:
        return evtNames.mousedownEvent;
    case JSEventTargetProperties::OnMouseMove:
        return evtNames.mousemoveEvent;
    case JSEventTargetProperties::OnMouseOut:
        return evtNames.mouseoutEvent;
    case JSEventTargetProperties::OnMouseOver:
        return evtNames.mouseoverEvent;
    case JSEventTargetProperties::OnMouseUp:
        return evtNames.mouseupEvent;      
    case JSEventTargetProperties::OnMouseWheel:
        return evtNames.mousewheelEvent;      
    case JSEventTargetProperties::OnBeforeCut:
        return evtNames.beforecutEvent;
    case JSEventTargetProperties::OnCut:
        return evtNames.cutEvent;
    case JSEventTargetProperties::OnBeforeCopy:
        return evtNames.beforecopyEvent;
    case JSEventTargetProperties::OnCopy:
        return evtNames.copyEvent;
    case JSEventTargetProperties::OnBeforePaste:
        return evtNames.beforepasteEvent;
    case JSEventTargetProperties::OnPaste:
        return evtNames.pasteEvent;
    case JSEventTargetProperties::OnDragEnter:
        return evtNames.dragenterEvent;
    case JSEventTargetProperties::OnDragOver:
        return evtNames.dragoverEvent;
    case JSEventTargetProperties::OnDragLeave:
        return evtNames.dragleaveEvent;
    case JSEventTargetProperties::OnDrop:
        return evtNames.dropEvent;
    case JSEventTargetProperties::OnDragStart:
        return evtNames.dragstartEvent;
    case JSEventTargetProperties::OnDrag:
        return evtNames.dragEvent;
    case JSEventTargetProperties::OnDragEnd:
        return evtNames.dragendEvent;
    case JSEventTargetProperties::OnReset:
        return evtNames.resetEvent;
    case JSEventTargetProperties::OnResize:
        return evtNames.resizeEvent;
    case JSEventTargetProperties::OnScroll:
        return evtNames.scrollEvent;
    case JSEventTargetProperties::OnSearch:
        return evtNames.searchEvent;
    case JSEventTargetProperties::OnSelect:
        return evtNames.selectEvent;
    case JSEventTargetProperties::OnSelectStart:
        return evtNames.selectstartEvent;
    case JSEventTargetProperties::OnSubmit:
        return evtNames.submitEvent;
    case JSEventTargetProperties::OnUnload:
        return evtNames.unloadEvent;
    }

    return nullAtom;
}

JSValue* toJS(ExecState* exec, EventTarget* target)
{
    if (!target)
        return jsNull();
    
#if ENABLE(SVG)
    // SVGElementInstance supports both toSVGElementInstance and toNode since so much mouse handling code depends on toNode returning a valid node.
    SVGElementInstance* instance = target->toSVGElementInstance();
    if (instance)
        return toJS(exec, instance);
#endif
    
    Node* node = target->toNode();
    if (node)
        return toJS(exec, node);

    if (XMLHttpRequest* xhr = target->toXMLHttpRequest())
        // XMLHttpRequest is always created via JS, so we don't need to use cacheDOMObject() here.
        return ScriptInterpreter::getDOMObject(xhr);

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    if (DOMApplicationCache* cache = target->toDOMApplicationCache())
        // DOMApplicationCache is always created via JS, so we don't need to use cacheDOMObject() here.
        return ScriptInterpreter::getDOMObject(cache);
#endif
    
    // There are two kinds of EventTargets: EventTargetNode and XMLHttpRequest.
    // If SVG support is enabled, there is also SVGElementInstance.
    ASSERT_NOT_REACHED();
    return jsNull();
}

} // namespace WebCore
