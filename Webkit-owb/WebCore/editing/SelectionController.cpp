/*
 * Copyright (C) 2004, 2008 Apple Inc. All rights reserved.
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
#include "SelectionController.h"

#include "CString.h"
#include "DeleteSelectionCommand.h"
#include "Document.h"
#include "Editor.h"
#include "Element.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "Page.h"
#include "Range.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "TextIterator.h"
#include "TypingCommand.h"
#include "htmlediting.h"
#include "visible_units.h"
#include <stdio.h>

#define EDIT_DEBUG 0

namespace WebCore {

using namespace HTMLNames;

const int NoXPosForVerticalArrowNavigation = INT_MIN;

SelectionController::SelectionController(Frame* frame, bool isDragCaretController)
    : m_needsLayout(true)
    , m_lastChangeWasHorizontalExtension(false)
    , m_frame(frame)
    , m_isDragCaretController(isDragCaretController)
    , m_isCaretBlinkingSuspended(false)
    , m_xPosForVerticalArrowNavigation(NoXPosForVerticalArrowNavigation)
    , m_focused(false)
{
}

void SelectionController::moveTo(const VisiblePosition &pos, bool userTriggered)
{
    setSelection(Selection(pos.deepEquivalent(), pos.deepEquivalent(), pos.affinity()), true, true, userTriggered);
}

void SelectionController::moveTo(const VisiblePosition &base, const VisiblePosition &extent, bool userTriggered)
{
    setSelection(Selection(base.deepEquivalent(), extent.deepEquivalent(), base.affinity()), true, true, userTriggered);
}

void SelectionController::moveTo(const Position &pos, EAffinity affinity, bool userTriggered)
{
    setSelection(Selection(pos, affinity), true, true, userTriggered);
}

void SelectionController::moveTo(const Range *r, EAffinity affinity, bool userTriggered)
{
    setSelection(Selection(startPosition(r), endPosition(r), affinity), true, true, userTriggered);
}

void SelectionController::moveTo(const Position &base, const Position &extent, EAffinity affinity, bool userTriggered)
{
    setSelection(Selection(base, extent, affinity), true, true, userTriggered);
}

void SelectionController::setSelection(const Selection& s, bool closeTyping, bool clearTypingStyleAndRemovedAnchor, bool userTriggered)
{
    if (m_isDragCaretController) {
        invalidateCaretRect();
        m_sel = s;
        m_needsLayout = true;
        invalidateCaretRect();
        return;
    }
    if (!m_frame) {
        m_sel = s;
        return;
    }
    
    if (s.base().node() && s.base().node()->document() != m_frame->document()) {
        s.base().node()->document()->frame()->selection()->setSelection(s, closeTyping, clearTypingStyleAndRemovedAnchor, userTriggered);
        return;
    }
    
    if (closeTyping)
        TypingCommand::closeTyping(m_frame->editor()->lastEditCommand());

    if (clearTypingStyleAndRemovedAnchor) {
        m_frame->clearTypingStyle();
        m_frame->editor()->setRemovedAnchor(0);
    }
        
    if (m_sel == s)
        return;
    
    Selection oldSelection = m_sel;

    m_sel = s;
    invalidateSelectionRect();

    m_needsLayout = true;
    
    if (!s.isNone())
        m_frame->setFocusedNodeIfNeeded();
    
    m_frame->selectionLayoutChanged();
    // Always clear the x position used for vertical arrow navigation.
    // It will be restored by the vertical arrow navigation code if necessary.
    m_xPosForVerticalArrowNavigation = NoXPosForVerticalArrowNavigation;
    selectFrameElementInParentIfFullySelected();
    m_frame->notifyRendererOfSelectionChange(userTriggered);
    m_frame->respondToChangedSelection(oldSelection, closeTyping);
    if (userTriggered)
        m_frame->revealCaret(RenderLayer::gAlignToEdgeIfNeeded);

    notifyAccessibilityForSelectionChange();
}

static bool removingNodeRemovesPosition(Node* node, const Position& position)
{
    if (!position.node())
        return false;
        
    if (position.node() == node)
        return true;
    
    if (!node->isElementNode())
        return false;
    
    Element* element = static_cast<Element*>(node);
    return element->contains(position.node()) || element->contains(position.node()->shadowAncestorNode());
}

void SelectionController::nodeWillBeRemoved(Node *node)
{
    if (isNone())
        return;
        
    // There can't be a selection inside a fragment, so if a fragment's node is being removed,
    // the selection in the document that created the fragment needs no adjustment.
    if (node && highestAncestor(node)->nodeType() == Node::DOCUMENT_FRAGMENT_NODE)
        return;
    
    bool baseRemoved = removingNodeRemovesPosition(node, m_sel.base());
    bool extentRemoved = removingNodeRemovesPosition(node, m_sel.extent());
    bool startRemoved = removingNodeRemovesPosition(node, m_sel.start());
    bool endRemoved = removingNodeRemovesPosition(node, m_sel.end());
    
    bool clearRenderTreeSelection = false;
    bool clearDOMTreeSelection = false;

    if (startRemoved || endRemoved) {
        // FIXME: When endpoints are removed, we should just alter the selection, instead of blowing it away.
        clearRenderTreeSelection = true;
        clearDOMTreeSelection = true;
    } else if (baseRemoved || extentRemoved) {
        // The base and/or extent are about to be removed, but the start and end aren't.
        // Change the base and extent to the start and end, but don't re-validate the
        // selection, since doing so could move the start and end into the node
        // that is about to be removed.
        if (m_sel.isBaseFirst())
            m_sel.setWithoutValidation(m_sel.start(), m_sel.end());
        else
            m_sel.setWithoutValidation(m_sel.end(), m_sel.start());
    // FIXME: This could be more efficient if we had an isNodeInRange function on Ranges.
    } else if (Range::compareBoundaryPoints(m_sel.start(), Position(node, 0)) == -1 &&
               Range::compareBoundaryPoints(m_sel.end(), Position(node, 0)) == 1) {
        // If we did nothing here, when this node's renderer was destroyed, the rect that it 
        // occupied would be invalidated, but, selection gaps that change as a result of 
        // the removal wouldn't be invalidated.
        // FIXME: Don't do so much unnecessary invalidation.
        clearRenderTreeSelection = true;
    }

    if (clearRenderTreeSelection) {
        RefPtr<Document> document = m_sel.start().node()->document();
        document->updateRendering();
        if (RenderView* view = static_cast<RenderView*>(document->renderer()))
            view->clearSelection();
    }

    if (clearDOMTreeSelection)
        setSelection(Selection(), false, false);
}

void SelectionController::willBeModified(EAlteration alter, EDirection direction)
{
    switch (alter) {
        case MOVE:
            m_lastChangeWasHorizontalExtension = false;
            break;
        case EXTEND:
            if (!m_lastChangeWasHorizontalExtension) {
                m_lastChangeWasHorizontalExtension = true;
                Position start = m_sel.start();
                Position end = m_sel.end();
                switch (direction) {
                    // FIXME: right for bidi?
                    case RIGHT:
                    case FORWARD:
                        m_sel.setBase(start);
                        m_sel.setExtent(end);
                        break;
                    case LEFT:
                    case BACKWARD:
                        m_sel.setBase(end);
                        m_sel.setExtent(start);
                        break;
                }
            }
            break;
    }
}

VisiblePosition SelectionController::modifyExtendingRightForward(TextGranularity granularity)
{
    VisiblePosition pos(m_sel.extent(), m_sel.affinity());
    switch (granularity) {
        case CharacterGranularity:
            pos = pos.next(true);
            break;
        case WordGranularity:
            pos = nextWordPosition(pos);
            break;
        case SentenceGranularity:
            pos = nextSentencePosition(pos);
            break;
        case LineGranularity:
            pos = nextLinePosition(pos, xPosForVerticalArrowNavigation(EXTENT));
            break;
        case ParagraphGranularity:
            pos = nextParagraphPosition(pos, xPosForVerticalArrowNavigation(EXTENT));
            break;
        case SentenceBoundary:
            pos = endOfSentence(VisiblePosition(m_sel.end(), m_sel.affinity()));
            break;
        case LineBoundary:
            pos = endOfLine(VisiblePosition(m_sel.end(), m_sel.affinity()));
            break;
        case ParagraphBoundary:
            pos = endOfParagraph(VisiblePosition(m_sel.end(), m_sel.affinity()));
            break;
        case DocumentBoundary:
            pos = VisiblePosition(m_sel.end(), m_sel.affinity());
            if (isEditablePosition(pos.deepEquivalent()))
                pos = endOfEditableContent(pos);
            else
                pos = endOfDocument(pos);
            break;
    }
    
    return pos;
}

VisiblePosition SelectionController::modifyMovingRight(TextGranularity granularity)
{
    VisiblePosition pos;
    switch (granularity) {
        case CharacterGranularity:
            if (isRange()) 
                pos = VisiblePosition(m_sel.end(), m_sel.affinity());
            else
                pos = VisiblePosition(m_sel.extent(), m_sel.affinity()).right(true);
            break;
        case WordGranularity:
        case SentenceGranularity:
        case LineGranularity:
        case ParagraphGranularity:
        case SentenceBoundary:
        case LineBoundary:
        case ParagraphBoundary:
        case DocumentBoundary:
            // FIXME: Implement all of the above.
            pos = modifyMovingForward(granularity);
            break;
    }
    return pos;
}

VisiblePosition SelectionController::modifyMovingForward(TextGranularity granularity)
{
    VisiblePosition pos;
    // FIXME: Stay in editable content for the less common granularities.
    switch (granularity) {
        case CharacterGranularity:
            if (isRange()) 
                pos = VisiblePosition(m_sel.end(), m_sel.affinity());
            else
                pos = VisiblePosition(m_sel.extent(), m_sel.affinity()).next(true);
            break;
        case WordGranularity:
            pos = nextWordPosition(VisiblePosition(m_sel.extent(), m_sel.affinity()));
            break;
        case SentenceGranularity:
            pos = nextSentencePosition(VisiblePosition(m_sel.extent(), m_sel.affinity()));
            break;
        case LineGranularity: {
            // down-arrowing from a range selection that ends at the start of a line needs
            // to leave the selection at that line start (no need to call nextLinePosition!)
            pos = VisiblePosition(m_sel.end(), m_sel.affinity());
            if (!isRange() || !isStartOfLine(pos))
                pos = nextLinePosition(pos, xPosForVerticalArrowNavigation(START));
            break;
        }
        case ParagraphGranularity:
            pos = nextParagraphPosition(VisiblePosition(m_sel.end(), m_sel.affinity()), xPosForVerticalArrowNavigation(START));
            break;
        case SentenceBoundary:
            pos = endOfSentence(VisiblePosition(m_sel.end(), m_sel.affinity()));
            break;
        case LineBoundary:
            pos = endOfLine(VisiblePosition(m_sel.end(), m_sel.affinity()));
            break;
        case ParagraphBoundary:
            pos = endOfParagraph(VisiblePosition(m_sel.end(), m_sel.affinity()));
            break;
        case DocumentBoundary:
            pos = VisiblePosition(m_sel.end(), m_sel.affinity());
            if (isEditablePosition(pos.deepEquivalent()))
                pos = endOfEditableContent(pos);
            else
                pos = endOfDocument(pos);
            break;
            
    }
    return pos;
}

VisiblePosition SelectionController::modifyExtendingLeftBackward(TextGranularity granularity)
{
    VisiblePosition pos(m_sel.extent(), m_sel.affinity());
        
    // Extending a selection backward by word or character from just after a table selects
    // the table.  This "makes sense" from the user perspective, esp. when deleting.
    // It was done here instead of in VisiblePosition because we want VPs to iterate
    // over everything.
    switch (granularity) {
        case CharacterGranularity:
            pos = pos.previous(true);
            break;
        case WordGranularity:
            pos = previousWordPosition(pos);
            break;
        case SentenceGranularity:
            pos = previousSentencePosition(pos);
            break;
        case LineGranularity:
            pos = previousLinePosition(pos, xPosForVerticalArrowNavigation(EXTENT));
            break;
        case ParagraphGranularity:
            pos = previousParagraphPosition(pos, xPosForVerticalArrowNavigation(EXTENT));
            break;
        case SentenceBoundary:
            pos = startOfSentence(VisiblePosition(m_sel.start(), m_sel.affinity()));
            break;
        case LineBoundary:
            pos = startOfLine(VisiblePosition(m_sel.start(), m_sel.affinity()));
            break;
        case ParagraphBoundary:
            pos = startOfParagraph(VisiblePosition(m_sel.start(), m_sel.affinity()));
            break;
        case DocumentBoundary:
            pos = VisiblePosition(m_sel.start(), m_sel.affinity());
            if (isEditablePosition(pos.deepEquivalent()))
                pos = startOfEditableContent(pos);
            else 
                pos = startOfDocument(pos);
            break;
    }
    return pos;
}

VisiblePosition SelectionController::modifyMovingLeft(TextGranularity granularity)
{
    VisiblePosition pos;
    switch (granularity) {
        case CharacterGranularity:
            if (isRange()) 
                pos = VisiblePosition(m_sel.start(), m_sel.affinity());
            else
                pos = VisiblePosition(m_sel.extent(), m_sel.affinity()).left(true);
            break;
        case WordGranularity:
        case SentenceGranularity:
        case LineGranularity:
        case ParagraphGranularity:
        case SentenceBoundary:
        case LineBoundary:
        case ParagraphBoundary:
        case DocumentBoundary:
            // FIXME: Implement all of the above.
            pos = modifyMovingBackward(granularity);
            break;
    }
    return pos;
}

VisiblePosition SelectionController::modifyMovingBackward(TextGranularity granularity)
{
    VisiblePosition pos;
    switch (granularity) {
        case CharacterGranularity:
            if (isRange()) 
                pos = VisiblePosition(m_sel.start(), m_sel.affinity());
            else
                pos = VisiblePosition(m_sel.extent(), m_sel.affinity()).previous(true);
            break;
        case WordGranularity:
            pos = previousWordPosition(VisiblePosition(m_sel.extent(), m_sel.affinity()));
            break;
        case SentenceGranularity:
            pos = previousSentencePosition(VisiblePosition(m_sel.extent(), m_sel.affinity()));
            break;
        case LineGranularity:
            pos = previousLinePosition(VisiblePosition(m_sel.start(), m_sel.affinity()), xPosForVerticalArrowNavigation(START));
            break;
        case ParagraphGranularity:
            pos = previousParagraphPosition(VisiblePosition(m_sel.start(), m_sel.affinity()), xPosForVerticalArrowNavigation(START));
            break;
        case SentenceBoundary:
            pos = startOfSentence(VisiblePosition(m_sel.start(), m_sel.affinity()));
            break;
        case LineBoundary:
            pos = startOfLine(VisiblePosition(m_sel.start(), m_sel.affinity()));
            break;
        case ParagraphBoundary:
            pos = startOfParagraph(VisiblePosition(m_sel.start(), m_sel.affinity()));
            break;
        case DocumentBoundary:
            pos = VisiblePosition(m_sel.start(), m_sel.affinity());
            if (isEditablePosition(pos.deepEquivalent()))
                pos = startOfEditableContent(pos);
            else 
                pos = startOfDocument(pos);
            break;
    }
    return pos;
}

bool SelectionController::modify(EAlteration alter, EDirection dir, TextGranularity granularity, bool userTriggered)
{
    if (userTriggered) {
        SelectionController trialSelectionController;
        trialSelectionController.setLastChangeWasHorizontalExtension(m_lastChangeWasHorizontalExtension);
        trialSelectionController.setSelection(m_sel);
        trialSelectionController.modify(alter, dir, granularity, false);

        bool change = m_frame->shouldChangeSelection(trialSelectionController.selection());
        if (!change)
            return false;
    }

    if (m_frame)
        m_frame->setSelectionGranularity(granularity);
    
    willBeModified(alter, dir);

    VisiblePosition pos;
    switch (dir) {
        case RIGHT:
            if (alter == MOVE)
                pos = modifyMovingRight(granularity);
            else
                pos = modifyExtendingRightForward(granularity);
            break;
        case FORWARD:
            if (alter == EXTEND)
                pos = modifyExtendingRightForward(granularity);
            else
                pos = modifyMovingForward(granularity);
            break;
        case LEFT:
            if (alter == MOVE)
                pos = modifyMovingLeft(granularity);
            else
                pos = modifyExtendingLeftBackward(granularity);
            break;
        case BACKWARD:
            if (alter == EXTEND)
                pos = modifyExtendingLeftBackward(granularity);
            else
                pos = modifyMovingBackward(granularity);
            break;
    }

    if (pos.isNull())
        return false;
    
    // Some of the above operations set an xPosForVerticalArrowNavigation.
    // Setting a selection will clear it, so save it to possibly restore later.
    // Note: the START position type is arbitrary because it is unused, it would be
    // the requested position type if there were no xPosForVerticalArrowNavigation set.
    int x = xPosForVerticalArrowNavigation(START);

    switch (alter) {
        case MOVE:
            moveTo(pos, userTriggered);
            break;
        case EXTEND:
            setExtent(pos, userTriggered);
            break;
    }
    
    if (granularity == LineGranularity || granularity == ParagraphGranularity)
        m_xPosForVerticalArrowNavigation = x;

    if (userTriggered) {
        // User modified selection change also sets the granularity back to character.
        // NOTE: The one exception is that we need to keep word granularity to
        // preserve smart delete behavior when extending by word (e.g. double-click),
        // then shift-option-right arrow, then delete needs to smart delete, per TextEdit.
        if (!(alter == EXTEND && granularity == WordGranularity && m_frame->selectionGranularity() == WordGranularity))
            m_frame->setSelectionGranularity(CharacterGranularity);
    }

    setNeedsLayout();

    return true;
}

// FIXME: Maybe baseline would be better?
static bool caretY(const VisiblePosition &c, int &y)
{
    IntRect rect = c.caretRect();
    if (rect.isEmpty())
        return false;
    y = rect.y() + rect.height() / 2;
    return true;
}

bool SelectionController::modify(EAlteration alter, int verticalDistance, bool userTriggered)
{
    if (verticalDistance == 0)
        return false;

    if (userTriggered) {
        SelectionController trialSelectionController;
        trialSelectionController.setSelection(m_sel);
        trialSelectionController.modify(alter, verticalDistance, false);

        bool change = m_frame->shouldChangeSelection(trialSelectionController.selection());
        if (!change)
            return false;
    }

    bool up = verticalDistance < 0;
    if (up)
        verticalDistance = -verticalDistance;

    willBeModified(alter, up ? BACKWARD : FORWARD);

    VisiblePosition pos;
    int xPos = 0;
    switch (alter) {
        case MOVE:
            pos = VisiblePosition(up ? m_sel.start() : m_sel.end(), m_sel.affinity());
            xPos = xPosForVerticalArrowNavigation(up ? START : END);
            m_sel.setAffinity(up ? UPSTREAM : DOWNSTREAM);
            break;
        case EXTEND:
            pos = VisiblePosition(m_sel.extent(), m_sel.affinity());
            xPos = xPosForVerticalArrowNavigation(EXTENT);
            m_sel.setAffinity(DOWNSTREAM);
            break;
    }

    int startY;
    if (!caretY(pos, startY))
        return false;
    if (up)
        startY = -startY;
    int lastY = startY;

    VisiblePosition result;
    VisiblePosition next;
    for (VisiblePosition p = pos; ; p = next) {
        next = (up ? previousLinePosition : nextLinePosition)(p, xPos);
        if (next.isNull() || next == p)
            break;
        int nextY;
        if (!caretY(next, nextY))
            break;
        if (up)
            nextY = -nextY;
        if (nextY - startY > verticalDistance)
            break;
        if (nextY >= lastY) {
            lastY = nextY;
            result = next;
        }
    }

    if (result.isNull())
        return false;

    switch (alter) {
        case MOVE:
            moveTo(result, userTriggered);
            break;
        case EXTEND:
            setExtent(result, userTriggered);
            break;
    }

    if (userTriggered)
        m_frame->setSelectionGranularity(CharacterGranularity);

    return true;
}

bool SelectionController::expandUsingGranularity(TextGranularity granularity)
{
    if (isNone())
        return false;

    m_sel.expandUsingGranularity(granularity);
    m_needsLayout = true;
    return true;
}

int SelectionController::xPosForVerticalArrowNavigation(EPositionType type)
{
    int x = 0;

    if (isNone())
        return x;

    Position pos;
    switch (type) {
        case START:
            pos = m_sel.start();
            break;
        case END:
            pos = m_sel.end();
            break;
        case BASE:
            pos = m_sel.base();
            break;
        case EXTENT:
            pos = m_sel.extent();
            break;
    }

    Frame *frame = pos.node()->document()->frame();
    if (!frame)
        return x;
        
    if (m_xPosForVerticalArrowNavigation == NoXPosForVerticalArrowNavigation) {
        VisiblePosition visiblePosition(pos, m_sel.affinity());
        // VisiblePosition creation can fail here if a node containing the selection becomes visibility:hidden
        // after the selection is created and before this function is called.
        x = visiblePosition.isNotNull() ? visiblePosition.caretRect().x() : 0;
        m_xPosForVerticalArrowNavigation = x;
    }
    else
        x = m_xPosForVerticalArrowNavigation;
        
    return x;
}

void SelectionController::clear()
{
    setSelection(Selection());
}

void SelectionController::setBase(const VisiblePosition &pos, bool userTriggered)
{
    setSelection(Selection(pos.deepEquivalent(), m_sel.extent(), pos.affinity()), true, true, userTriggered);
}

void SelectionController::setExtent(const VisiblePosition &pos, bool userTriggered)
{
    setSelection(Selection(m_sel.base(), pos.deepEquivalent(), pos.affinity()), true, true, userTriggered);
}

void SelectionController::setBase(const Position &pos, EAffinity affinity, bool userTriggered)
{
    setSelection(Selection(pos, m_sel.extent(), affinity), true, true, userTriggered);
}

void SelectionController::setExtent(const Position &pos, EAffinity affinity, bool userTriggered)
{
    setSelection(Selection(m_sel.base(), pos, affinity), true, true, userTriggered);
}

void SelectionController::setNeedsLayout(bool flag)
{
    m_needsLayout = flag;
}

void SelectionController::layout()
{
    if (isNone() || !m_sel.start().node()->inDocument() || !m_sel.end().node()->inDocument()) {
        m_caretRect = IntRect();
        m_caretPositionOnLayout = IntPoint();
        return;
    }

    m_sel.start().node()->document()->updateRendering();
    
    m_caretRect = IntRect();
    m_caretPositionOnLayout = IntPoint();
        
    if (isCaret()) {
        VisiblePosition pos(m_sel.start(), m_sel.affinity());
        if (pos.isNotNull()) {
            ASSERT(pos.deepEquivalent().node()->renderer());
            m_caretRect = pos.caretRect();

            int x, y;
            pos.deepEquivalent().node()->renderer()->absolutePositionForContent(x, y);
            m_caretPositionOnLayout = IntPoint(x, y);
        }
    }

    m_needsLayout = false;
}

IntRect SelectionController::caretRect() const
{
    if (m_needsLayout)
        const_cast<SelectionController *>(this)->layout();
    
    IntRect caret = m_caretRect;

    if (m_sel.start().node() && m_sel.start().node()->renderer()) {
        int x, y;
        m_sel.start().node()->renderer()->absolutePositionForContent(x, y);
        caret.move(IntPoint(x, y) - m_caretPositionOnLayout);
    }

    return caret;
}

static IntRect repaintRectForCaret(IntRect caret)
{
    if (caret.isEmpty())
        return IntRect();
    // Ensure that the dirty rect intersects the block that paints the caret even in the case where
    // the caret itself is just outside the block. See <https://bugs.webkit.org/show_bug.cgi?id=19086>.
    caret.inflateX(1);
    return caret;
}

IntRect SelectionController::caretRepaintRect() const
{
    return repaintRectForCaret(caretRect());
}

bool SelectionController::recomputeCaretRect()
{
    if (!m_frame || !m_frame->document())
        return false;
        
    FrameView* v = m_frame->document()->view();
    if (!v)
        return false;

    if (!m_needsLayout)
        return false;

    IntRect oldRect = m_caretRect;
    m_needsLayout = true;
    IntRect newRect = caretRect();
    if (oldRect == newRect)
        return false;

    v->updateContents(repaintRectForCaret(oldRect), false);
    v->updateContents(repaintRectForCaret(newRect), false);
    return true;
}

void SelectionController::invalidateCaretRect()
{
    if (!isCaret())
        return;
    bool caretRectChanged = recomputeCaretRect();

    // EDIT FIXME: This is an unfortunate hack.
    // Basically, we can't trust this layout position since we 
    // can't guarantee that the check to see if we are in unrendered 
    // content will work at this point. We may have to wait for
    // a layout and re-render of the document to happen. So, resetting this
    // flag will cause another caret layout to happen the first time
    // that we try to paint the caret after this call. That one will work since
    // it happens after the document has accounted for any editing
    // changes which may have been done.
    // And, we need to leave this layout here so the caret moves right 
    // away after clicking.
    m_needsLayout = true;
    if (!caretRectChanged)
        invalidateRect(caretRepaintRect());
}

void SelectionController::invalidateSelectionRect()
{
    //Nicki Vankoughnett:  Ideally I would just obtain a rect containing the highlighted page
    //elements, and pass that to SelectionController::invalidateRect().  However, there
    //are presently bugs with the partial screen redraw process.  For now I will just
    //update the containing view.
    if(m_frame != NULL &&  m_frame->document() != NULL && m_frame->document()->view() != NULL)
        m_frame->document()->view()->update();
}

void SelectionController::invalidateRect(const IntRect& r)
{
    FrameView* v = m_sel.start().node()->document()->view();
    if (v)
        v->updateContents(r, false);
}


void SelectionController::paintCaret(GraphicsContext *p, const IntRect &rect)
{
    if (! m_sel.isCaret())
        return;

    if (m_needsLayout)
        layout();
        
    IntRect caret = intersection(caretRect(), rect);
    if (!caret.isEmpty()) {
        Color caretColor = Color::black;
        Element* element = rootEditableElement();
        if (element && element->renderer())
            caretColor = element->renderer()->style()->color();

        p->fillRect(caret, caretColor);
    }
}

void SelectionController::debugRenderer(RenderObject *r, bool selected) const
{
    if (r->node()->isElementNode()) {
        Element *element = static_cast<Element *>(r->node());
        fprintf(stderr, "%s%s\n", selected ? "==> " : "    ", element->localName().string().utf8().data());
    }
    else if (r->isText()) {
        RenderText* textRenderer = static_cast<RenderText*>(r);
        if (textRenderer->textLength() == 0 || !textRenderer->firstTextBox()) {
            fprintf(stderr, "%s#text (empty)\n", selected ? "==> " : "    ");
            return;
        }
        
        static const int max = 36;
        String text = textRenderer->text();
        int textLength = text.length();
        if (selected) {
            int offset = 0;
            if (r->node() == m_sel.start().node())
                offset = m_sel.start().offset();
            else if (r->node() == m_sel.end().node())
                offset = m_sel.end().offset();
                
            int pos;
            InlineTextBox *box = textRenderer->findNextInlineTextBox(offset, pos);
            text = text.substring(box->m_start, box->m_len);
            
            String show;
            int mid = max / 2;
            int caret = 0;
            
            // text is shorter than max
            if (textLength < max) {
                show = text;
                caret = pos;
            }
            
            // too few characters to left
            else if (pos - mid < 0) {
                show = text.left(max - 3) + "...";
                caret = pos;
            }
            
            // enough characters on each side
            else if (pos - mid >= 0 && pos + mid <= textLength) {
                show = "..." + text.substring(pos - mid + 3, max - 6) + "...";
                caret = mid;
            }
            
            // too few characters on right
            else {
                show = "..." + text.right(max - 3);
                caret = pos - (textLength - show.length());
            }
            
            show.replace('\n', ' ');
            show.replace('\r', ' ');
            fprintf(stderr, "==> #text : \"%s\" at offset %d\n", show.utf8().data(), pos);
            fprintf(stderr, "           ");
            for (int i = 0; i < caret; i++)
                fprintf(stderr, " ");
            fprintf(stderr, "^\n");
        }
        else {
            if ((int)text.length() > max)
                text = text.left(max - 3) + "...";
            else
                text = text.left(max);
            fprintf(stderr, "    #text : \"%s\"\n", text.utf8().data());
        }
    }
}

bool SelectionController::contains(const IntPoint& point)
{
    Document* document = m_frame->document();
    
    // Treat a collapsed selection like no selection.
    if (!isRange())
        return false;
    if (!document->renderer()) 
        return false;
    
    HitTestRequest request(true, true);
    HitTestResult result(point);
    document->renderer()->layer()->hitTest(request, result);
    Node* innerNode = result.innerNode();
    if (!innerNode || !innerNode->renderer())
        return false;
    
    VisiblePosition visiblePos(innerNode->renderer()->positionForPoint(result.localPoint()));
    if (visiblePos.isNull())
        return false;
        
    if (m_sel.visibleStart().isNull() || m_sel.visibleEnd().isNull())
        return false;
        
    Position start(m_sel.visibleStart().deepEquivalent());
    Position end(m_sel.visibleEnd().deepEquivalent());
    Position p(visiblePos.deepEquivalent());

    return comparePositions(start, p) <= 0 && comparePositions(p, end) <= 0;
}

// Workaround for the fact that it's hard to delete a frame.
// Call this after doing user-triggered selections to make it easy to delete the frame you entirely selected.
// Can't do this implicitly as part of every setSelection call because in some contexts it might not be good
// for the focus to move to another frame. So instead we call it from places where we are selecting with the
// mouse or the keyboard after setting the selection.
void SelectionController::selectFrameElementInParentIfFullySelected()
{
    // Find the parent frame; if there is none, then we have nothing to do.
    Frame* parent = m_frame->tree()->parent();
    if (!parent)
        return;
    Page* page = m_frame->page();
    if (!page)
        return;

    // Check if the selection contains the entire frame contents; if not, then there is nothing to do.
    if (!isRange())
        return;
    if (!isStartOfDocument(selection().visibleStart()))
        return;
    if (!isEndOfDocument(selection().visibleEnd()))
        return;

    // Get to the <iframe> or <frame> (or even <object>) element in the parent frame.
    Document* doc = m_frame->document();
    if (!doc)
        return;
    Element* ownerElement = doc->ownerElement();
    if (!ownerElement)
        return;
    Node* ownerElementParent = ownerElement->parentNode();
    if (!ownerElementParent)
        return;
        
    // This method's purpose is it to make it easier to select iframes (in order to delete them).  Don't do anything if the iframe isn't deletable.
    if (!ownerElementParent->isContentEditable())
        return;

    // Create compute positions before and after the element.
    unsigned ownerElementNodeIndex = ownerElement->nodeIndex();
    VisiblePosition beforeOwnerElement(VisiblePosition(ownerElementParent, ownerElementNodeIndex, SEL_DEFAULT_AFFINITY));
    VisiblePosition afterOwnerElement(VisiblePosition(ownerElementParent, ownerElementNodeIndex + 1, VP_UPSTREAM_IF_POSSIBLE));

    // Focus on the parent frame, and then select from before this element to after.
    Selection newSelection(beforeOwnerElement, afterOwnerElement);
    if (parent->shouldChangeSelection(newSelection)) {
        page->focusController()->setFocusedFrame(parent);
        parent->selection()->setSelection(newSelection);
    }
}

void SelectionController::selectAll()
{
    Document* document = m_frame->document();
    if (!document)
        return;
    
    if (document->focusedNode() && document->focusedNode()->canSelectAll()) {
        document->focusedNode()->selectAll();
        return;
    }
    
    Node* root = isContentEditable() ? highestEditableRoot(m_sel.start()) : document->documentElement();
    if (!root)
        return;
    Selection newSelection(Selection::selectionFromContentsOfNode(root));
    if (m_frame->shouldChangeSelection(newSelection))
        setSelection(newSelection);
    selectFrameElementInParentIfFullySelected();
    m_frame->notifyRendererOfSelectionChange(true);
}

bool SelectionController::setSelectedRange(Range* range, EAffinity affinity, bool closeTyping)
{
    if (!range)
        return false;

    ExceptionCode ec = 0;
    Node* startContainer = range->startContainer(ec);
    if (ec)
        return false;

    Node* endContainer = range->endContainer(ec);
    if (ec)
        return false;
    
    ASSERT(startContainer);
    ASSERT(endContainer);
    ASSERT(startContainer->document() == endContainer->document());
    
    m_frame->document()->updateLayoutIgnorePendingStylesheets();

    // Non-collapsed ranges are not allowed to start at the end of a line that is wrapped,
    // they start at the beginning of the next line instead
    bool collapsed = range->collapsed(ec);
    if (ec)
        return false;
    
    int startOffset = range->startOffset(ec);
    if (ec)
        return false;

    int endOffset = range->endOffset(ec);
    if (ec)
        return false;
    
    // FIXME: Can we provide extentAffinity?
    VisiblePosition visibleStart(startContainer, startOffset, collapsed ? affinity : DOWNSTREAM);
    VisiblePosition visibleEnd(endContainer, endOffset, SEL_DEFAULT_AFFINITY);
    setSelection(Selection(visibleStart, visibleEnd), closeTyping);
    return true;
}

bool SelectionController::isInPasswordField() const
{
    Node* startNode = start().node();
    if (!startNode)
        return false;

    startNode = startNode->shadowAncestorNode();
    if (!startNode)
        return false;

    if (!startNode->hasTagName(inputTag))
        return false;
    
    return static_cast<HTMLInputElement*>(startNode)->inputType() == HTMLInputElement::PASSWORD;
}

bool SelectionController::isInsideNode() const
{
    Node* startNode = start().node();
    if (!startNode)
        return false;
    return !isTableElement(startNode) && !editingIgnoresContent(startNode);
}

void SelectionController::focusedOrActiveStateChanged()
{
    bool activeAndFocused = isFocusedAndActive();

    // Because RenderObject::selectionBackgroundColor() and
    // RenderObject::selectionForegroundColor() check if the frame is active,
    // we have to update places those colors were painted.
    if (m_frame->view())
        m_frame->view()->updateContents(enclosingIntRect(m_frame->selectionRect()));

    // Caret appears in the active frame.
    if (activeAndFocused)
        m_frame->setSelectionFromNone();
    m_frame->setCaretVisible(activeAndFocused);

    // Update for caps lock state
    m_frame->eventHandler()->capsLockStateMayHaveChanged();

    // Because CSSStyleSelector::checkOneSelector() and
    // RenderTheme::isFocused() check if the frame is active, we have to
    // update style and theme state that depended on those.
    if (Node* node = m_frame->document()->focusedNode()) {
        node->setChanged();
        if (RenderObject* renderer = node->renderer())
            if (renderer && renderer->style()->hasAppearance())
                theme()->stateChanged(renderer, FocusState);
    }

    // Secure keyboard entry is set by the active frame.
    if (m_frame->document()->useSecureKeyboardEntryWhenActive())
        m_frame->setUseSecureKeyboardEntry(activeAndFocused);
}

void SelectionController::pageActivationChanged()
{
    focusedOrActiveStateChanged();
}

void SelectionController::setFocused(bool flag)
{
    if (m_focused == flag)
        return;
    m_focused = flag;

    focusedOrActiveStateChanged();

    if (Document* doc = m_frame->document())
        doc->dispatchWindowEvent(flag ? eventNames().focusEvent : eventNames().blurEvent, false, false);
}

bool SelectionController::isFocusedAndActive() const
{
    return m_focused && m_frame->page() && m_frame->page()->focusController()->isActive();
}
  
#ifndef NDEBUG

void SelectionController::formatForDebugger(char* buffer, unsigned length) const
{
    m_sel.formatForDebugger(buffer, length);
}

void SelectionController::showTreeForThis() const
{
    m_sel.showTreeForThis();
}

#endif

}

#ifndef NDEBUG

void showTree(const WebCore::SelectionController& sel)
{
    sel.showTreeForThis();
}

void showTree(const WebCore::SelectionController* sel)
{
    if (sel)
        sel->showTreeForThis();
}

#endif
