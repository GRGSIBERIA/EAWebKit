/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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

#ifndef EventHandler_h
#define EventHandler_h

#include <wtf/FastAllocBase.h>
#include "DragActions.h"
#include "FocusDirection.h"
#include "PlatformMouseEvent.h"
#include "ScrollTypes.h"
#include "Timer.h"
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/Platform.h>
#include <wtf/RefPtr.h>

#if PLATFORM(MAC)
#include "WebCoreKeyboardUIMode.h"
#ifndef __OBJC__
class NSEvent;
class NSView;
#endif
#endif

namespace WebCore {

class AtomicString;
class Clipboard;
class Cursor;
class EventTargetNode;
class Event;
class FloatPoint;
class FloatRect;
class Frame;
class HitTestResult;
class HTMLFrameSetElement;
class KeyboardEvent;
class MouseEventWithHitTestResults;
class Node;
class PlatformKeyboardEvent;
class PlatformScrollbar;
class PlatformWheelEvent;
class RenderLayer;
class RenderObject;
class RenderWidget;
class String;
class TextEvent;
class VisiblePosition;
class Widget;
    
struct HitTestRequest;

extern const int LinkDragHysteresis;
extern const int ImageDragHysteresis;
extern const int TextDragHysteresis;
extern const int GeneralDragHysteresis;

class EventHandler : Noncopyable {
public:
// Placement operator new.
void* operator new(size_t, void* p) { return p; }
void* operator new[](size_t, void* p) { return p; }
 
void* operator new(size_t size)
{
     void* p = fastMalloc(size);
     fastMallocMatchValidateMalloc(p, WTF::Internal::AllocTypeClassNew);
     return p;
}
 
void operator delete(void* p)
{
     fastMallocMatchValidateFree(p, WTF::Internal::AllocTypeClassNew);
     fastFree(p);  // We don't need to check for a null pointer; the compiler does this.
}
 
void* operator new[](size_t size)
{
     void* p = fastMalloc(size);
     fastMallocMatchValidateMalloc(p, WTF::Internal::AllocTypeClassNewArray);
     return p;
}
 
void operator delete[](void* p)
{
     fastMallocMatchValidateFree(p, WTF::Internal::AllocTypeClassNewArray);
     fastFree(p);  // We don't need to check for a null pointer; the compiler does this.
}
public:
    EventHandler(Frame*);
    ~EventHandler();

    void clear();

    void updateSelectionForMouseDrag();

    Node* mousePressNode() const;
    void setMousePressNode(PassRefPtr<Node>);

    void stopAutoscrollTimer(bool rendererIsBeingDestroyed = false);
    RenderObject* autoscrollRenderer() const;

    HitTestResult hitTestResultAtPoint(const IntPoint&, bool allowShadowContent);


    //These functions work with the WebCore::MouseButton enums defined in BCPlatformMouseEventEA.h,
    //NoButton, LeftButton, MiddleButton, or RightButton
    void setMousePressed(WebCore::MouseButton btn, bool pressed);
    bool mousePressed(WebCore::MouseButton btn);
    bool anyMousePressed();

    void setCapturingMouseEventsNode(PassRefPtr<Node>);

    bool updateDragAndDrop(const PlatformMouseEvent&, Clipboard*);
    void cancelDragAndDrop(const PlatformMouseEvent&, Clipboard*);
    bool performDragAndDrop(const PlatformMouseEvent&, Clipboard*);

    void scheduleHoverStateUpdate();

    void setResizingFrameSet(HTMLFrameSetElement*);

    void resizeLayerDestroyed();

    IntPoint currentMousePosition() const;

    void setIgnoreWheelEvents(bool);

    bool scrollOverflow(ScrollDirection, ScrollGranularity);

    bool shouldDragAutoNode(Node*, const IntPoint&) const; // -webkit-user-drag == auto

    bool tabsToLinks(KeyboardEvent*) const;
    bool tabsToAllControls(KeyboardEvent*) const;

    bool mouseDownMayStartSelect() const { return m_mouseDownMayStartSelect; }

    bool mouseMoved(const PlatformMouseEvent&);

    bool handleMousePressEvent(const PlatformMouseEvent&);
    bool handleMouseMoveEvent(const PlatformMouseEvent&, HitTestResult* hoveredNode = 0);
    bool handleMouseReleaseEvent(const PlatformMouseEvent&);
    bool handleWheelEvent(PlatformWheelEvent&);

    bool sendContextMenuEvent(const PlatformMouseEvent&);

    void setMouseDownMayStartAutoscroll() { m_mouseDownMayStartAutoscroll = true; }

    bool needsKeyboardEventDisambiguationQuirks() const;

    static unsigned accessKeyModifiers() { return s_accessKeyModifiers; }
    bool handleAccessKey(const PlatformKeyboardEvent&);
    bool keyEvent(const PlatformKeyboardEvent&);
    void defaultKeyboardEventHandler(KeyboardEvent*);

    bool handleTextInputEvent(const String& text, Event* underlyingEvent = 0,
        bool isLineBreak = false, bool isBackTab = false);
    void defaultTextInputEventHandler(TextEvent*);

    bool eventMayStartDrag(const PlatformMouseEvent&) const;
    
    void dragSourceMovedTo(const PlatformMouseEvent&);
    void dragSourceEndedAt(const PlatformMouseEvent&, DragOperation);

    void focusDocumentView();

    void capsLockStateMayHaveChanged();
    
    unsigned pendingFrameUnloadEventCount();
    void addPendingFrameUnloadEventCount();
    void removePendingFrameUnloadEventCount();
    void clearPendingFrameUnloadEventCount();
    unsigned pendingFrameBeforeUnloadEventCount();
    void addPendingFrameBeforeUnloadEventCount();
    void removePendingFrameBeforeUnloadEventCount();
    void clearPendingFrameBeforeUnloadEventCount();
    
#if PLATFORM(MAC)
    PassRefPtr<KeyboardEvent> currentKeyboardEvent() const;

    void mouseDown(NSEvent*);
    void mouseDragged(NSEvent*);
    void mouseUp(NSEvent*);
    void mouseMoved(NSEvent*);
    bool keyEvent(NSEvent*);
    bool wheelEvent(NSEvent*);

    void sendFakeEventsAfterWidgetTracking(NSEvent* initiatingEvent);

    void setActivationEventNumber(int num) { m_activationEventNumber = num; }

    NSEvent *currentNSEvent();
#endif

private:
    struct EventHandlerDragState: public WTF::FastAllocBase {
        RefPtr<Node> m_dragSrc; // element that may be a drag source, for the current mouse gesture
        bool m_dragSrcIsLink;
        bool m_dragSrcIsImage;
        bool m_dragSrcInSelection;
        bool m_dragSrcMayBeDHTML;
        bool m_dragSrcMayBeUA; // Are DHTML and/or the UserAgent allowed to drag out?
        bool m_dragSrcIsDHTML;
        RefPtr<Clipboard> m_dragClipboard; // used on only the source side of dragging
    };
    static EventHandlerDragState& dragState();
    static const double TextDragDelay;
    
    PassRefPtr<Clipboard> createDraggingClipboard() const;
    
    bool eventActivatedView(const PlatformMouseEvent&) const;
    void selectClosestWordFromMouseEvent(const MouseEventWithHitTestResults& event);
    void selectClosestWordOrLinkFromMouseEvent(const MouseEventWithHitTestResults& event);

    bool handleMouseDoubleClickEvent(const PlatformMouseEvent&);

    bool handleMousePressEvent(const MouseEventWithHitTestResults&);
    bool handleMousePressEventSingleClick(const MouseEventWithHitTestResults&);
    bool handleMousePressEventDoubleClick(const MouseEventWithHitTestResults&);
    bool handleMousePressEventTripleClick(const MouseEventWithHitTestResults&);
    bool handleMouseDraggedEvent(const MouseEventWithHitTestResults&);
    bool handleMouseReleaseEvent(const MouseEventWithHitTestResults&);

    void handleKeyboardSelectionMovement(KeyboardEvent*);
    
    Cursor selectCursor(const MouseEventWithHitTestResults&, PlatformScrollbar*);

    void hoverTimerFired(Timer<EventHandler>*);

    static bool canMouseDownStartSelect(Node*);
    static bool canMouseDragExtendSelect(Node*);

    void handleAutoscroll(RenderObject*);
    void startAutoscrollTimer();
    void setAutoscrollRenderer(RenderObject*);

    void autoscrollTimerFired(Timer<EventHandler>*);

    void invalidateClick();

    Node* nodeUnderMouse() const;
    
    void updateMouseEventTargetNode(Node*, const PlatformMouseEvent&, bool fireMouseOverOut);
    void fireMouseOverOut(bool fireMouseOver = true, bool fireMouseOut = true, bool updateLastNodeUnderMouse = true);
    
    MouseEventWithHitTestResults prepareMouseEvent(const HitTestRequest&, const PlatformMouseEvent&);

    bool dispatchMouseEvent(const AtomicString& eventType, Node* target, bool cancelable, int clickCount, const PlatformMouseEvent&, bool setUnder);
    bool dispatchDragEvent(const AtomicString& eventType, Node* target, const PlatformMouseEvent&, Clipboard*);

    void freeClipboard();

    bool handleDrag(const MouseEventWithHitTestResults&);
    bool handleMouseUp(const MouseEventWithHitTestResults&);
    void clearDragState();

    bool dispatchDragSrcEvent(const AtomicString& eventType, const PlatformMouseEvent&);

    bool dragHysteresisExceeded(const FloatPoint&) const;
    bool dragHysteresisExceeded(const IntPoint&) const;

    bool passMousePressEventToSubframe(MouseEventWithHitTestResults&, Frame* subframe);
    bool passMouseMoveEventToSubframe(MouseEventWithHitTestResults&, Frame* subframe, HitTestResult* hoveredNode = 0);
    bool passMouseReleaseEventToSubframe(MouseEventWithHitTestResults&, Frame* subframe);

    bool passSubframeEventToSubframe(MouseEventWithHitTestResults&, Frame* subframe, HitTestResult* hoveredNode = 0);

    bool passMousePressEventToScrollbar(MouseEventWithHitTestResults&, PlatformScrollbar*);

    bool passWidgetMouseDownEventToWidget(const MouseEventWithHitTestResults&);
    bool passWidgetMouseDownEventToWidget(RenderWidget*);

    bool passMouseDownEventToWidget(Widget*);
    bool passWheelEventToWidget(PlatformWheelEvent&, Widget*);

    void defaultTabEventHandler(KeyboardEvent*);

    void allowDHTMLDrag(bool& flagDHTML, bool& flagUA) const;

    // The following are called at the beginning of handleMouseUp and handleDrag.  
    // If they return true it indicates that they have consumed the event.
#if PLATFORM(MAC)
    bool eventLoopHandleMouseUp(const MouseEventWithHitTestResults&);
    bool eventLoopHandleMouseDragged(const MouseEventWithHitTestResults&);
    NSView *mouseDownViewIfStillGood();
#else
    bool eventLoopHandleMouseUp(const MouseEventWithHitTestResults&) { return false; }
    bool eventLoopHandleMouseDragged(const MouseEventWithHitTestResults&) { return false; }
#endif

    bool invertSenseOfTabsToLinks(KeyboardEvent*) const;

    void updateSelectionForMouseDrag(Node* targetNode, const IntPoint& localPoint);

    Frame* m_frame;

    bool m_mouseBtns[3];    //correspond to the WebCore::MouseButton enums in BCPlatformMouseEventEA.h, 
    RefPtr<Node> m_mousePressNode;

    bool m_mouseDownMayStartSelect;
    bool m_mouseDownMayStartDrag;
    bool m_mouseDownWasSingleClickInSelection;
    bool m_beganSelectingText;

    IntPoint m_dragStartPos;

    Timer<EventHandler> m_hoverTimer;
    
    Timer<EventHandler> m_autoscrollTimer;
    RenderObject* m_autoscrollRenderer;
    bool m_mouseDownMayStartAutoscroll;
    bool m_mouseDownWasInSubframe;
#if ENABLE(SVG)
    bool m_svgPan;
#endif

    RenderLayer* m_resizeLayer;

    RefPtr<Node> m_capturingMouseEventsNode;
    
    RefPtr<Node> m_nodeUnderMouse;
    RefPtr<Node> m_lastNodeUnderMouse;
    RefPtr<Frame> m_lastMouseMoveEventSubframe;
    RefPtr<PlatformScrollbar> m_lastScrollbarUnderMouse;

    int m_clickCount;
    RefPtr<Node> m_clickNode;

    RefPtr<Node> m_dragTarget;
    
    RefPtr<HTMLFrameSetElement> m_frameSetBeingResized;

    IntSize m_offsetFromResizeCorner;    
    
    IntPoint m_currentMousePosition;
    IntPoint m_mouseDownPos; // in our view's coords
    double m_mouseDownTimestamp;
    PlatformMouseEvent m_mouseDown;

    static unsigned s_accessKeyModifiers;
    
    unsigned m_pendingFrameUnloadEventCount;
    unsigned m_pendingFrameBeforeUnloadEventCount;

#if PLATFORM(MAC)
    NSView *m_mouseDownView;
    bool m_sendingEventToSubview;
    int m_activationEventNumber;
#endif

};

} // namespace WebCore

#endif // EventHandler_h
