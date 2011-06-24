/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *           (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

/*
* This file was modified by Electronic Arts Inc Copyright � 2009
*/

#include "config.h"
#include "EventTargetNode.h"

#include "Document.h"
#include "EventException.h"
#include "EventHandler.h"
#include "EventListener.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameView.h"
#include "KeyboardEvent.h"
#include "MouseEvent.h"
#include "MutationEvent.h"
#include "Page.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "ProgressEvent.h"
#include "RegisteredEventListener.h"
#include "TextEvent.h"
#include "WheelEvent.h"
#include <wtf/HashSet.h>

#if ENABLE(DOM_STORAGE)
#include "StorageEvent.h"
#endif

namespace WebCore {

static HashSet<EventTargetNode*>* gNodesDispatchingSimulatedClicks = 0; 

EventTargetNode::EventTargetNode(Document *doc)
    : Node(doc)
    , m_regdListeners(0)
{
}

EventTargetNode::~EventTargetNode()
{
    if (m_regdListeners && !m_regdListeners->isEmpty() && !inDocument())
        document()->unregisterDisconnectedNodeWithEventListeners(this);

    delete m_regdListeners;
    m_regdListeners = 0;
}

// 9/4/09 CSidhall - Added leak fix 
void EventTargetNode::staticFinalize()
{
    if(gNodesDispatchingSimulatedClicks) {
        delete gNodesDispatchingSimulatedClicks;
        gNodesDispatchingSimulatedClicks = NULL;
    }
}

void EventTargetNode::insertedIntoDocument()
{
    EventTarget::insertedIntoDocument(this);
    Node::insertedIntoDocument();
}

void EventTargetNode::removedFromDocument()
{
    EventTarget::removedFromDocument(this);
    Node::removedFromDocument();
}

void EventTargetNode::willMoveToNewOwnerDocument()
{
    EventTarget::willMoveToNewOwnerDocument(this);
    Node::willMoveToNewOwnerDocument();
}

void EventTargetNode::didMoveToNewOwnerDocument()
{
    EventTarget::didMoveToNewOwnerDocument(this);
    Node::didMoveToNewOwnerDocument();
}

void EventTargetNode::addEventListener(const AtomicString& eventType, PassRefPtr<EventListener> listener, bool useCapture)
{
    EventTarget::addEventListener(this, eventType, listener, useCapture);
}

void EventTargetNode::removeEventListener(const AtomicString& eventType, EventListener* listener, bool useCapture)
{
    EventTarget::removeEventListener(this, eventType, listener, useCapture);
}

void EventTargetNode::removeAllEventListeners()
{
    EventTarget::removeAllEventListeners(this);
}

void EventTargetNode::handleLocalEvents(Event *evt, bool useCapture)
{
    if (disabled() && evt->isMouseEvent())
        return;

    EventTarget::handleLocalEvents(this, evt, useCapture);    
}

bool EventTargetNode::dispatchEvent(PassRefPtr<Event> e, ExceptionCode& ec, bool tempEvent)
{
    RefPtr<Event> evt(e);
    ASSERT(!eventDispatchForbidden());
    if (!evt || evt->type().isEmpty()) { 
        ec = EventException::UNSPECIFIED_EVENT_TYPE_ERR;
        return false;
    }

    EventTargetNode* eventTarget = this;
    evt->setTarget(eventTargetRespectingSVGTargetRules(eventTarget));

    RefPtr<FrameView> view = document()->view();
    return dispatchGenericEvent(eventTarget, evt.release(), ec, tempEvent);
}

bool EventTargetNode::dispatchSubtreeModifiedEvent()
{
    ASSERT(!eventDispatchForbidden());
    
    document()->incDOMTreeVersion();

    notifyNodeListsAttributeChanged(); // FIXME: Can do better some day. Really only care about the name attribute changing.
    
    if (!document()->hasListenerType(Document::DOMSUBTREEMODIFIED_LISTENER))
        return false;
    ExceptionCode ec = 0;
    return dispatchEvent(MutationEvent::create(eventNames().DOMSubtreeModifiedEvent, true, false, 0, String(), String(), String(), 0), ec, true);
}

void EventTargetNode::dispatchWindowEvent(PassRefPtr<Event> e)
{
    ASSERT(!eventDispatchForbidden());
    RefPtr<Event> evt(e);
    RefPtr<Document> doc = document();
    evt->setTarget(doc);
    doc->handleWindowEvent(evt.get(), true);
    doc->handleWindowEvent(evt.get(), false);
}

void EventTargetNode::dispatchWindowEvent(const AtomicString& eventType, bool canBubbleArg, bool cancelableArg)
{
    ASSERT(!eventDispatchForbidden());
    RefPtr<Document> doc = document();
    dispatchWindowEvent(Event::create(eventType, canBubbleArg, cancelableArg));
    
    if (eventType == eventNames().loadEvent) {
        // For onload events, send a separate load event to the enclosing frame only.
        // This is a DOM extension and is independent of bubbling/capturing rules of
        // the DOM.
        Element* ownerElement = doc->ownerElement();
        if (ownerElement) {
            RefPtr<Event> ownerEvent = Event::create(eventType, false, cancelableArg);
            ownerEvent->setTarget(ownerElement);
            ExceptionCode ec = 0;
            ownerElement->dispatchGenericEvent(ownerElement, ownerEvent.release(), ec, true);
        }
    }
}

bool EventTargetNode::dispatchUIEvent(const AtomicString& eventType, int detail, PassRefPtr<Event> underlyingEvent)
{
    ASSERT(!eventDispatchForbidden());
    ASSERT(eventType == eventNames().DOMFocusInEvent || eventType == eventNames().DOMFocusOutEvent || eventType == eventNames().DOMActivateEvent);
    
    bool cancelable = eventType == eventNames().DOMActivateEvent;
    
    ExceptionCode ec = 0;
    RefPtr<UIEvent> evt = UIEvent::create(eventType, true, cancelable, document()->defaultView(), detail);
    evt->setUnderlyingEvent(underlyingEvent);
    return dispatchEvent(evt.release(), ec, true);
}

bool EventTargetNode::dispatchKeyEvent(const PlatformKeyboardEvent& key)
{
    ASSERT(!eventDispatchForbidden());
    ExceptionCode ec = 0;
    RefPtr<KeyboardEvent> keyboardEvent = KeyboardEvent::create(key, document()->defaultView());
    bool r = dispatchEvent(keyboardEvent,ec,true);
    
    // we want to return false if default is prevented (already taken care of)
    // or if the element is default-handled by the DOM. Otherwise we let it just
    // let it get handled by AppKit 
    if (keyboardEvent->defaultHandled())
        r = false;
    
    return r;
}

bool EventTargetNode::dispatchMouseEvent(const PlatformMouseEvent& event, const AtomicString& eventType,
    int detail, Node* relatedTarget)
{
    ASSERT(!eventDispatchForbidden());
    
    IntPoint contentsPos;
    if (FrameView* view = document()->view())
        contentsPos = view->windowToContents(event.pos());

    short button = event.button();

    ASSERT(event.eventType() == MouseEventMoved || button != NoButton);
    
    return dispatchMouseEvent(eventType, button, detail,
        contentsPos.x(), contentsPos.y(), event.globalX(), event.globalY(),
        event.ctrlKey(), event.altKey(), event.shiftKey(), event.metaKey(),
        false, relatedTarget);
}

void EventTargetNode::dispatchSimulatedMouseEvent(const AtomicString& eventType,
    PassRefPtr<Event> underlyingEvent)
{
    ASSERT(!eventDispatchForbidden());

    bool ctrlKey = false;
    bool altKey = false;
    bool shiftKey = false;
    bool metaKey = false;
    if (UIEventWithKeyState* keyStateEvent = findEventWithKeyState(underlyingEvent.get())) {
        ctrlKey = keyStateEvent->ctrlKey();
        altKey = keyStateEvent->altKey();
        shiftKey = keyStateEvent->shiftKey();
        metaKey = keyStateEvent->metaKey();
    }

    // Like Gecko, we just pass 0 for everything when we make a fake mouse event.
    // Internet Explorer instead gives the current mouse position and state.
    dispatchMouseEvent(eventType, 0, 0, 0, 0, 0, 0,
        ctrlKey, altKey, shiftKey, metaKey, true, 0, underlyingEvent);
}

void EventTargetNode::dispatchSimulatedClick(PassRefPtr<Event> event, bool sendMouseEvents, bool showPressedLook)
{
    if (!gNodesDispatchingSimulatedClicks)
        gNodesDispatchingSimulatedClicks = new HashSet<EventTargetNode*>;
    else if (gNodesDispatchingSimulatedClicks->contains(this))
        return;
    
    gNodesDispatchingSimulatedClicks->add(this);
    
    // send mousedown and mouseup before the click, if requested
    if (sendMouseEvents)
        dispatchSimulatedMouseEvent(eventNames().mousedownEvent, event.get());
    setActive(true, showPressedLook);
    if (sendMouseEvents)
        dispatchSimulatedMouseEvent(eventNames().mouseupEvent, event.get());
    setActive(false);

    // always send click
    dispatchSimulatedMouseEvent(eventNames().clickEvent, event);
    
    gNodesDispatchingSimulatedClicks->remove(this);
}

bool EventTargetNode::dispatchMouseEvent(const AtomicString& eventType, int button, int detail,
    int pageX, int pageY, int screenX, int screenY,
    bool ctrlKey, bool altKey, bool shiftKey, bool metaKey, 
    bool isSimulated, Node* relatedTargetArg, PassRefPtr<Event> underlyingEvent)
{
    ASSERT(!eventDispatchForbidden());
    if (disabled()) // Don't even send DOM events for disabled controls..
        return true;
    
    if (eventType.isEmpty())
        return false; // Shouldn't happen.
    
    // Dispatching the first event can easily result in this node being destroyed.
    // Since we dispatch up to three events here, we need to make sure we're referenced
    // so the pointer will be good for the two subsequent ones.
    RefPtr<Node> protect(this);
    
    bool cancelable = eventType != eventNames().mousemoveEvent;
    
    ExceptionCode ec = 0;
    
    bool swallowEvent = false;
    
    // Attempting to dispatch with a non-EventTarget relatedTarget causes the relatedTarget to be silently ignored.
    RefPtr<EventTargetNode> relatedTarget = (relatedTargetArg && relatedTargetArg->isEventTargetNode())
        ? static_cast<EventTargetNode*>(relatedTargetArg) : 0;

    RefPtr<Event> mouseEvent = MouseEvent::create(eventType,
        true, cancelable, document()->defaultView(),
        detail, screenX, screenY, pageX, pageY,
        ctrlKey, altKey, shiftKey, metaKey, button,
        relatedTarget, 0, isSimulated);
    mouseEvent->setUnderlyingEvent(underlyingEvent.get());
    
    dispatchEvent(mouseEvent, ec, true);
    bool defaultHandled = mouseEvent->defaultHandled();
    bool defaultPrevented = mouseEvent->defaultPrevented();
    if (defaultHandled || defaultPrevented)
        swallowEvent = true;
    
    // Special case: If it's a double click event, we also send the dblclick event. This is not part
    // of the DOM specs, but is used for compatibility with the ondblclick="" attribute.  This is treated
    // as a separate event in other DOM-compliant browsers like Firefox, and so we do the same.
    if (eventType == eventNames().clickEvent && detail == 2) {
        RefPtr<Event> doubleClickEvent = MouseEvent::create(eventNames().dblclickEvent,
            true, cancelable, document()->defaultView(),
            detail, screenX, screenY, pageX, pageY,
            ctrlKey, altKey, shiftKey, metaKey, button,
            relatedTarget, 0, isSimulated);
        doubleClickEvent->setUnderlyingEvent(underlyingEvent.get());
        if (defaultHandled)
            doubleClickEvent->setDefaultHandled();
        dispatchEvent(doubleClickEvent, ec, true);
        if (doubleClickEvent->defaultHandled() || doubleClickEvent->defaultPrevented())
            swallowEvent = true;
    }

    return swallowEvent;
}

void EventTargetNode::dispatchWheelEvent(PlatformWheelEvent& e)
{
    ASSERT(!eventDispatchForbidden());
    if (e.deltaX() == 0 && e.deltaY() == 0)
        return;
    
    FrameView* view = document()->view();
    if (!view)
        return;
    
    IntPoint pos = view->windowToContents(e.pos());
    
    RefPtr<WheelEvent> we = WheelEvent::create(e.deltaX(), e.deltaY(),
        document()->defaultView(), e.globalX(), e.globalY(), pos.x(), pos.y(),
        e.ctrlKey(), e.altKey(), e.shiftKey(), e.metaKey());
    ExceptionCode ec = 0;
    if (!dispatchEvent(we.release(), ec, true))
        e.accept();
}


void EventTargetNode::dispatchFocusEvent()
{
    dispatchHTMLEvent(eventNames().focusEvent, false, false);
}

void EventTargetNode::dispatchBlurEvent()
{
    dispatchHTMLEvent(eventNames().blurEvent, false, false);
}

bool EventTargetNode::dispatchHTMLEvent(const AtomicString &eventType, bool canBubbleArg, bool cancelableArg)
{
    ASSERT(!eventDispatchForbidden());
    ExceptionCode ec = 0;
    return dispatchEvent(Event::create(eventType, canBubbleArg, cancelableArg), ec, true);
}

bool EventTargetNode::dispatchProgressEvent(const AtomicString &eventType, bool lengthComputableArg, unsigned loadedArg, unsigned totalArg)
{
    ASSERT(!eventDispatchForbidden());
    ExceptionCode ec = 0;
    return dispatchEvent(ProgressEvent::create(eventType, lengthComputableArg, loadedArg, totalArg), ec, true);
}

void EventTargetNode::dispatchStorageEvent(const AtomicString &eventType, const String& key, const String& oldValue, const String& newValue, Frame* source)
{
#if ENABLE(DOM_STORAGE)
    ASSERT(!eventDispatchForbidden());
    ExceptionCode ec = 0;
    dispatchEvent(StorageEvent::create(eventType, key, oldValue, newValue, source->document()->documentURI(), source->domWindow()), ec, true); 
#endif
}

void EventTargetNode::removeHTMLEventListener(const AtomicString &eventType)
{
    if (!m_regdListeners) // nothing to remove
        return;
    
    RegisteredEventListenerList::Iterator end = m_regdListeners->end();
    for (RegisteredEventListenerList::Iterator it = m_regdListeners->begin(); it != end; ++it)
        if ((*it)->eventType() == eventType && (*it)->listener()->isHTMLEventListener()) {
            it = m_regdListeners->remove(it);
            // removed last
            if (m_regdListeners->isEmpty() && !inDocument())
                document()->unregisterDisconnectedNodeWithEventListeners(this);
            return;
        }
}

void EventTargetNode::setHTMLEventListener(const AtomicString &eventType, PassRefPtr<EventListener> listener)
{
    // In case we are the only one holding a reference to it, we don't want removeHTMLEventListener to destroy it.
    removeHTMLEventListener(eventType);
    if (listener)
        addEventListener(eventType, listener, false);
}

EventListener *EventTargetNode::getHTMLEventListener(const AtomicString &eventType)
{
    if (!m_regdListeners)
        return 0;
    
    RegisteredEventListenerList::Iterator end = m_regdListeners->end();
    for (RegisteredEventListenerList::Iterator it = m_regdListeners->begin(); it != end; ++it)
        if ((*it)->eventType() == eventType && (*it)->listener()->isHTMLEventListener())
            return (*it)->listener();
    return 0;
}

bool EventTargetNode::disabled() const
{
    return false;
}

void EventTargetNode::defaultEventHandler(Event* event)
{
    if (event->target() != this)
        return;
    const AtomicString& eventType = event->type();
    if (eventType == eventNames().keydownEvent || eventType == eventNames().keypressEvent) {
        if (event->isKeyboardEvent())
            if (Frame* frame = document()->frame())
                frame->eventHandler()->defaultKeyboardEventHandler(static_cast<KeyboardEvent*>(event));
    } else if (eventType == eventNames().clickEvent) {
        int detail = event->isUIEvent() ? static_cast<UIEvent*>(event)->detail() : 0;
        dispatchUIEvent(eventNames().DOMActivateEvent, detail, event);
    } else if (eventType == eventNames().contextmenuEvent) {
        if (Frame* frame = document()->frame())
            if (Page* page = frame->page())
                page->contextMenuController()->handleContextMenuEvent(event);
    } else if (eventType == eventNames().textInputEvent) {
        if (event->isTextEvent())
            if (Frame* frame = document()->frame())
                frame->eventHandler()->defaultTextInputEventHandler(static_cast<TextEvent*>(event));
    }
}

} // namespace WebCore
