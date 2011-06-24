/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 *           (C) 2004 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef RenderObject_h
#define RenderObject_h

#include <wtf/FastAllocBase.h>
#include "CachedResourceClient.h"
#include "Document.h"
#include "RenderStyle.h"
#include "ScrollTypes.h"
#include "VisiblePosition.h"
#include <wtf/HashMap.h>

namespace WebCore {

class AffineTransform;
class AnimationController;
class Color;
class Document;
class Element;
class Event;
class FloatRect;
class FrameView;
class HTMLAreaElement;
class HitTestResult;
class InlineBox;
class InlineFlowBox;
class PlatformScrollbar;
class Position;
class RenderArena;
class RenderBlock;
class RenderFlow;
class RenderFrameSet;
class RenderLayer;
class RenderTable;
class RenderText;
class RenderView;
class String;

struct HitTestRequest;

/*
 *  The painting of a layer occurs in three distinct phases.  Each phase involves
 *  a recursive descent into the layer's render objects. The first phase is the background phase.
 *  The backgrounds and borders of all blocks are painted.  Inlines are not painted at all.
 *  Floats must paint above block backgrounds but entirely below inline content that can overlap them.
 *  In the foreground phase, all inlines are fully painted.  Inline replaced elements will get all
 *  three phases invoked on them during this phase.
 */

enum PaintPhase {
    PaintPhaseBlockBackground,
    PaintPhaseChildBlockBackground,
    PaintPhaseChildBlockBackgrounds,
    PaintPhaseFloat,
    PaintPhaseForeground,
    PaintPhaseOutline,
    PaintPhaseChildOutlines,
    PaintPhaseSelfOutline,
    PaintPhaseSelection,
    PaintPhaseCollapsedTableBorders,
    PaintPhaseTextClip,
    PaintPhaseMask
};

enum PaintRestriction {
    PaintRestrictionNone,
    PaintRestrictionSelectionOnly,
    PaintRestrictionSelectionOnlyBlackText
};

enum HitTestFilter {
    HitTestAll,
    HitTestSelf,
    HitTestDescendants
};

enum HitTestAction {
    HitTestBlockBackground,
    HitTestChildBlockBackground,
    HitTestChildBlockBackgrounds,
    HitTestFloat,
    HitTestForeground
};

enum VerticalPositionHint {
    PositionTop = -0x7fffffff,
    PositionBottom = 0x7fffffff,
    PositionUndefined = static_cast<int>(0x80000000)
};

#if ENABLE(DASHBOARD_SUPPORT)
struct DashboardRegionValue: public WTF::FastAllocBase {
    bool operator==(const DashboardRegionValue& o) const
    {
        return type == o.type && bounds == o.bounds && clip == o.clip && label == o.label;
    }
    bool operator!=(const DashboardRegionValue& o) const
    {
        return !(*this == o);
    }

    String label;
    IntRect bounds;
    IntRect clip;
    int type;
};
#endif

// FIXME: This should be a HashSequencedSet, but we don't have that data structure yet.
// This means the paint order of outlines will be wrong, although this is a minor issue.
typedef HashSet<RenderFlow*> RenderFlowSequencedSet;

// Base class for all rendering tree objects.
class RenderObject : public CachedResourceClient {
    friend class RenderContainer;
    friend class RenderSVGContainer;
    friend class RenderLayer;
public:
    // Anonymous objects should pass the document as their node, and they will then automatically be
    // marked as anonymous in the constructor.
    RenderObject(Node*);
    virtual ~RenderObject();

    virtual const char* renderName() const { return "RenderObject"; }

    RenderObject* parent() const { return m_parent; }
    bool isDescendantOf(const RenderObject*) const;

    RenderObject* previousSibling() const { return m_previous; }
    RenderObject* nextSibling() const { return m_next; }

    virtual RenderObject* firstChild() const { return 0; }
    virtual RenderObject* lastChild() const { return 0; }

    RenderObject* nextInPreOrder() const;
    RenderObject* nextInPreOrder(RenderObject* stayWithin) const;
    RenderObject* nextInPreOrderAfterChildren() const;
    RenderObject* nextInPreOrderAfterChildren(RenderObject* stayWithin) const;
    RenderObject* previousInPreOrder() const;
    RenderObject* childAt(unsigned) const;

    RenderObject* firstLeafChild() const;
    RenderObject* lastLeafChild() const;

    virtual RenderLayer* layer() const { return 0; }
    RenderLayer* enclosingLayer() const;
    void addLayers(RenderLayer* parentLayer, RenderObject* newObject);
    void removeLayers(RenderLayer* parentLayer);
    void moveLayers(RenderLayer* oldParent, RenderLayer* newParent);
    RenderLayer* findNextLayer(RenderLayer* parentLayer, RenderObject* startPoint, bool checkParent = true);
    virtual void positionChildLayers() { }
    virtual bool requiresLayer();

    virtual IntRect getOverflowClipRect(int /*tx*/, int /*ty*/) { return IntRect(0, 0, 0, 0); }
    virtual IntRect getClipRect(int /*tx*/, int /*ty*/) { return IntRect(0, 0, 0, 0); }
    bool hasClip() { return isPositioned() && style()->hasClip(); }

    virtual int getBaselineOfFirstLineBox() const { return -1; }
    virtual int getBaselineOfLastLineBox() const { return -1; }

    virtual bool isEmpty() const { return firstChild() == 0; }

    virtual bool isEdited() const { return false; }
    virtual void setEdited(bool) { }

#ifndef NDEBUG
    void setHasAXObject(bool flag) { m_hasAXObject = flag; }
    bool hasAXObject() const { return m_hasAXObject; }
#endif

    // Obtains the nearest enclosing block (including this block) that contributes a first-line style to our inline
    // children.
    virtual RenderBlock* firstLineBlock() const;

    // Called when an object that was floating or positioned becomes a normal flow object
    // again.  We have to make sure the render tree updates as needed to accommodate the new
    // normal flow object.
    void handleDynamicFloatPositionChange();

    // This function is a convenience helper for creating an anonymous block that inherits its
    // style from this RenderObject.
    RenderBlock* createAnonymousBlock();

    // Whether or not a positioned element requires normal flow x/y to be computed
    // to determine its position.
    bool hasStaticX() const;
    bool hasStaticY() const;
    virtual void setStaticX(int /*staticX*/) { }
    virtual void setStaticY(int /*staticY*/) { }
    virtual int staticX() const { return 0; }
    virtual int staticY() const { return 0; }

    // RenderObject tree manipulation
    //////////////////////////////////////////
    virtual bool canHaveChildren() const;
    virtual bool isChildAllowed(RenderObject*, RenderStyle*) const { return true; }
    virtual void addChild(RenderObject* newChild, RenderObject* beforeChild = 0);
    virtual void removeChild(RenderObject*);
    virtual bool createsAnonymousWrapper() const { return false; }

    // raw tree manipulation
    virtual RenderObject* removeChildNode(RenderObject*, bool fullRemove = true);
    virtual void appendChildNode(RenderObject*, bool fullAppend = true);
    virtual void insertChildNode(RenderObject* child, RenderObject* before, bool fullInsert = true);
    // Designed for speed.  Don't waste time doing a bunch of work like layer updating and repainting when we know that our
    // change in parentage is not going to affect anything.
    virtual void moveChildNode(RenderObject*);
    //////////////////////////////////////////

protected:
    //////////////////////////////////////////
    // Helper functions. Dangerous to use!
    void setPreviousSibling(RenderObject* previous) { m_previous = previous; }
    void setNextSibling(RenderObject* next) { m_next = next; }
    void setParent(RenderObject* parent) { m_parent = parent; }
    //////////////////////////////////////////
private:
    void addAbsoluteRectForLayer(IntRect& result);

public:
#ifndef NDEBUG
    void showTreeForThis() const;
#endif

    static RenderObject* createObject(Node*, RenderStyle*);

    // Overloaded new operator.  Derived classes must override operator new
    // in order to allocate out of the RenderArena.
    void* operator new(size_t, RenderArena*) throw();

    // Overridden to prevent the normal delete from being called.
    void operator delete(void*, size_t);

private:
    // The normal operator new is disallowed on all render objects.
    void* operator new(size_t) throw();

public:
    RenderArena* renderArena() const { return document()->renderArena(); }

    virtual bool isApplet() const { return false; }
    virtual bool isBR() const { return false; }
    virtual bool isBlockFlow() const { return false; }
    virtual bool isCounter() const { return false; }
    virtual bool isFrame() const { return false; }
    virtual bool isFrameSet() const { return false; }
    virtual bool isImage() const { return false; }
    virtual bool isInlineBlockOrInlineTable() const { return false; }
    virtual bool isInlineContinuation() const;
    virtual bool isInlineFlow() const { return false; }
    virtual bool isListBox() const { return false; }
    virtual bool isListItem() const { return false; }
    virtual bool isListMarker() const { return false; }
    virtual bool isMedia() const { return false; }
    virtual bool isMenuList() const { return false; }
    virtual bool isRenderBlock() const { return false; }
    virtual bool isRenderImage() const { return false; }
    virtual bool isRenderInline() const { return false; }
    virtual bool isRenderPart() const { return false; }
    virtual bool isRenderView() const { return false; }
    virtual bool isSlider() const { return false; }
    virtual bool isTable() const { return false; }
    virtual bool isTableCell() const { return false; }
    virtual bool isTableCol() const { return false; }
    virtual bool isTableRow() const { return false; }
    virtual bool isTableSection() const { return false; }
    virtual bool isTextArea() const { return false; }
    virtual bool isTextField() const { return false; }
    virtual bool isWidget() const { return false; }


    bool isRoot() const { return document()->documentElement() == node(); }
    bool isBody() const;
    bool isHR() const;

    bool isHTMLMarquee() const;

    virtual bool childrenInline() const { return false; }
    virtual void setChildrenInline(bool) { }

    virtual RenderFlow* continuation() const;

#if ENABLE(SVG)
    virtual bool isSVGRoot() const { return false; }
    virtual bool isSVGContainer() const { return false; }
    virtual bool isSVGHiddenContainer() const { return false; }
    virtual bool isRenderPath() const { return false; }
    virtual bool isSVGText() const { return false; }

    virtual FloatRect relativeBBox(bool includeStroke = true) const;

    virtual AffineTransform localTransform() const;
    virtual AffineTransform absoluteTransform() const;
#endif

    virtual bool isEditable() const;

    bool isAnonymous() const { return m_isAnonymous; }
    void setIsAnonymous(bool b) { m_isAnonymous = b; }
    bool isAnonymousBlock() const
    {
        return m_isAnonymous && style()->display() == BLOCK && style()->styleType() == RenderStyle::NOPSEUDO && !isListMarker();
    }

    bool isFloating() const { return m_floating; }
    bool isPositioned() const { return m_positioned; } // absolute or fixed positioning
    bool isRelPositioned() const { return m_relPositioned; } // relative positioning
    bool isText() const  { return m_isText; }
    bool isInline() const { return m_inline; }  // inline object
    bool isCompact() const { return style()->display() == COMPACT; } // compact object
    bool isRunIn() const { return style()->display() == RUN_IN; } // run-in object
    bool isDragging() const { return m_isDragging; }
    bool isReplaced() const { return m_replaced; } // a "replaced" element (see CSS)
    
    bool hasLayer() const { return m_hasLayer; }
    
    bool hasBoxDecorations() const { return m_paintBackground; }
    bool mustRepaintBackgroundOrBorder() const;

    bool hasHorizontalBordersPaddingOrMargin() const { return hasHorizontalBordersOrPadding() || marginLeft() != 0 || marginRight() != 0; }
    bool hasHorizontalBordersOrPadding() const { return borderLeft() != 0 || borderRight() != 0 || paddingLeft() != 0 || paddingRight() != 0; }
                                                              
    bool needsLayout() const { return m_needsLayout || m_normalChildNeedsLayout || m_posChildNeedsLayout || m_needsPositionedMovementLayout; }
    bool selfNeedsLayout() const { return m_needsLayout; }
    bool needsPositionedMovementLayout() const { return m_needsPositionedMovementLayout; }
    bool needsPositionedMovementLayoutOnly() const { return m_needsPositionedMovementLayout && !m_needsLayout && !m_normalChildNeedsLayout && !m_posChildNeedsLayout; }
    bool posChildNeedsLayout() const { return m_posChildNeedsLayout; }
    bool normalChildNeedsLayout() const { return m_normalChildNeedsLayout; }
    
    bool prefWidthsDirty() const { return m_prefWidthsDirty; }

    bool isSelectionBorder() const;

    bool hasOverflowClip() const { return m_hasOverflowClip; }
    virtual bool hasControlClip() const { return false; }
    virtual IntRect controlClipRect(int /*tx*/, int /*ty*/) const { return IntRect(); }

    bool hasAutoVerticalScrollbar() const { return hasOverflowClip() && (style()->overflowY() == OAUTO || style()->overflowY() == OOVERLAY); }
    bool hasAutoHorizontalScrollbar() const { return hasOverflowClip() && (style()->overflowX() == OAUTO || style()->overflowX() == OOVERLAY); }

    bool scrollsOverflow() const { return scrollsOverflowX() || scrollsOverflowY(); }
    bool scrollsOverflowX() const { return hasOverflowClip() && (style()->overflowX() == OSCROLL || hasAutoHorizontalScrollbar()); }
    bool scrollsOverflowY() const { return hasOverflowClip() && (style()->overflowY() == OSCROLL || hasAutoVerticalScrollbar()); }

    virtual int verticalScrollbarWidth() const;
    virtual int horizontalScrollbarHeight() const;
    
    bool hasTransform() const { return m_hasTransform; }
    bool hasMask() const { return style() && style()->hasMask(); }
    virtual IntRect maskClipRect() { return borderBox(); }

private:
    bool includeVerticalScrollbarSize() const { return hasOverflowClip() && (style()->overflowY() == OSCROLL || style()->overflowY() == OAUTO); }
    bool includeHorizontalScrollbarSize() const { return hasOverflowClip() && (style()->overflowX() == OSCROLL || style()->overflowX() == OAUTO); }

public:
    // The pseudo element style can be cached or uncached.  Use the cached method if the pseudo element doesn't respect
    // any pseudo classes (and therefore has no concept of changing state).
    RenderStyle* getCachedPseudoStyle(RenderStyle::PseudoId, RenderStyle* parentStyle = 0) const;
    PassRefPtr<RenderStyle> getUncachedPseudoStyle(RenderStyle::PseudoId, RenderStyle* parentStyle = 0) const;
    
    void updateDragState(bool dragOn);

    RenderView* view() const;

    // don't even think about making this method virtual!
    Node* element() const { return m_isAnonymous ? 0 : m_node; }
    Document* document() const { return m_node->document(); }
    void setNode(Node* node) { m_node = node; }
    Node* node() const { return m_node; }

    bool hasOutlineAnnotation() const;
    bool hasOutline() const { return style()->hasOutline() || hasOutlineAnnotation(); }

   /**
     * returns the object containing this one. can be different from parent for
     * positioned elements
     */
    RenderObject* container() const;
    RenderObject* hoverAncestor() const;

    virtual void markAllDescendantsWithFloatsForLayout(RenderObject* floatToRemove = 0);
    void markContainingBlocksForLayout(bool scheduleRelayout = true, RenderObject* newRoot = 0);
    void setNeedsLayout(bool b, bool markParents = true);
    void setChildNeedsLayout(bool b, bool markParents = true);
    void setNeedsPositionedMovementLayout();
    void setPrefWidthsDirty(bool, bool markParents = true);
    void invalidateContainerPrefWidths();
    
    void setNeedsLayoutAndPrefWidthsRecalc()
    {
        setNeedsLayout(true);
        setPrefWidthsDirty(true);
    }

    void setPositioned(bool b = true)  { m_positioned = b;  }
    void setRelPositioned(bool b = true) { m_relPositioned = b; }
    void setFloating(bool b = true) { m_floating = b; }
    void setInline(bool b = true) { m_inline = b; }
    void setHasBoxDecorations(bool b = true) { m_paintBackground = b; }
    void setRenderText() { m_isText = true; }
    void setReplaced(bool b = true) { m_replaced = b; }
    void setHasOverflowClip(bool b = true) { m_hasOverflowClip = b; }
    void setHasLayer(bool b = true) { m_hasLayer = b; }
    void setHasTransform(bool b = true) { m_hasTransform = b; }
    void setHasReflection(bool b = true) { m_hasReflection = b; }

    void scheduleRelayout();

    void updateFillImages(const FillLayer*, const FillLayer*);
    void updateImage(StyleImage*, StyleImage*);

    virtual InlineBox* createInlineBox(bool makePlaceHolderBox, bool isRootLineBox, bool isOnlyRun = false);
    virtual void dirtyLineBoxes(bool fullLayout, bool isRootLineBox = false);

    // For inline replaced elements, this function returns the inline box that owns us.  Enables
    // the replaced RenderObject to quickly determine what line it is contained on and to easily
    // iterate over structures on the line.
    virtual InlineBox* inlineBoxWrapper() const;
    virtual void setInlineBoxWrapper(InlineBox*);
    virtual void deleteLineBoxWrapper();

    // for discussion of lineHeight see CSS2 spec
    virtual int lineHeight(bool firstLine, bool isRootLineBox = false) const;
    // for the vertical-align property of inline elements
    // the difference between this objects baseline position and the lines baseline position.
    virtual int verticalPositionHint(bool firstLine) const;
    // the offset of baseline from the top of the object.
    virtual int baselinePosition(bool firstLine, bool isRootLineBox = false) const;

    /*
     * Paint the object and its children, clipped by (x|y|w|h).
     * (tx|ty) is the calculated position of the parent
     */
    struct PaintInfo: public WTF::FastAllocBase {
        PaintInfo(GraphicsContext* newContext, const IntRect& newRect, PaintPhase newPhase, bool newForceBlackText,
                  RenderObject* newPaintingRoot, RenderFlowSequencedSet* newOutlineObjects)
            : context(newContext)
            , rect(newRect)
            , phase(newPhase)
            , forceBlackText(newForceBlackText)
            , paintingRoot(newPaintingRoot)
            , outlineObjects(newOutlineObjects)
        {
        }

        GraphicsContext* context;
        IntRect rect;
        PaintPhase phase;
        bool forceBlackText;
        RenderObject* paintingRoot; // used to draw just one element and its visual kids
        RenderFlowSequencedSet* outlineObjects; // used to list outlines that should be painted by a block with inline children
    };

    virtual void paint(PaintInfo&, int tx, int ty);
    void paintBorder(GraphicsContext*, int tx, int ty, int w, int h, const RenderStyle*, bool begin = true, bool end = true);
    bool paintNinePieceImage(GraphicsContext*, int tx, int ty, int w, int h, const RenderStyle*, const NinePieceImage&, CompositeOperator = CompositeSourceOver);
    void paintOutline(GraphicsContext*, int tx, int ty, int w, int h, const RenderStyle*);
    void paintBoxShadow(GraphicsContext*, int tx, int ty, int w, int h, const RenderStyle*, bool begin = true, bool end = true);

    // RenderBox implements this.
    virtual void paintBoxDecorations(PaintInfo&, int tx, int ty) { }
    virtual void paintMask(PaintInfo&, int tx, int ty) { }
    virtual void paintFillLayerExtended(const PaintInfo&, const Color&, const FillLayer*,
                                        int clipy, int cliph, int tx, int ty, int width, int height,
                                        InlineFlowBox* box = 0, CompositeOperator = CompositeSourceOver) { }

    
    /*
     * Calculates the actual width of the object (only for non inline
     * objects)
     */
    virtual void calcWidth() { }

    /*
     * This function should cause the Element to calculate its
     * width and height and the layout of its content
     *
     * when the Element calls setNeedsLayout(false), layout() is no
     * longer called during relayouts, as long as there is no
     * style sheet change. When that occurs, m_needsLayout will be
     * set to true and the Element receives layout() calls
     * again.
     */
    virtual void layout() = 0;

    /* This function performs a layout only if one is needed. */
    void layoutIfNeeded() { if (needsLayout()) layout(); }

    // Called when a positioned object moves but doesn't change size.  A simplified layout is done
    // that just updates the object's position.
    virtual void layoutDoingPositionedMovementOnly() {};
    
    // used for element state updates that cannot be fixed with a
    // repaint and do not need a relayout
    virtual void updateFromElement() { }

    // Block flows subclass availableWidth to handle multi column layout (shrinking the width available to children when laying out.)
    virtual int availableWidth() const { return contentWidth(); }
    
    virtual int availableHeight() const { return 0; }

    virtual void updateWidgetPosition();

#if ENABLE(DASHBOARD_SUPPORT)
    void addDashboardRegions(Vector<DashboardRegionValue>&);
    void collectDashboardRegions(Vector<DashboardRegionValue>&);
#endif

    // Used to signal a specific subrect within an object that must be repainted after
    // layout is complete.
    struct RepaintInfo: public WTF::FastAllocBase {
        RepaintInfo(RenderObject* object = 0, const IntRect& repaintRect = IntRect())
            : m_object(object)
            , m_repaintRect(repaintRect)
        {
        }

        RenderObject* m_object;
        IntRect m_repaintRect;
    };

    bool hitTest(const HitTestRequest&, HitTestResult&, const IntPoint&, int tx, int ty, HitTestFilter = HitTestAll);
    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, int x, int y, int tx, int ty, HitTestAction);
    void updateHitTestResult(HitTestResult&, const IntPoint&);

    virtual VisiblePosition positionForCoordinates(int x, int y);
    VisiblePosition positionForPoint(const IntPoint& point) { return positionForCoordinates(point.x(), point.y()); }

    virtual void dirtyLinesFromChangedChild(RenderObject*);

    // Called to update a style that is allowed to trigger animations.
    // FIXME: Right now this will typically be called only when updating happens from the DOM on explicit elements.
    // We don't yet handle generated content animation such as first-letter or before/after (we'll worry about this later).
    void setAnimatableStyle(PassRefPtr<RenderStyle>);

    // Set the style of the object and update the state of the object accordingly.
    virtual void setStyle(PassRefPtr<RenderStyle>);

    // Updates only the local style ptr of the object.  Does not update the state of the object,
    // and so only should be called when the style is known not to have changed (or from setStyle).
    void setStyleInternal(PassRefPtr<RenderStyle>);

    // returns the containing block level element for this element.
    RenderBlock* containingBlock() const;

    // return just the width of the containing block
    virtual int containingBlockWidth() const;
    // return just the height of the containing block
    virtual int containingBlockHeight() const;

    // content area (box minus padding/border)
    IntRect contentBox() const;
    IntRect absoluteContentBox() const;
    int contentWidth() const { return clientWidth() - paddingLeft() - paddingRight(); }
    int contentHeight() const { return clientHeight() - paddingTop() - paddingBottom(); }

    // used by flexible boxes to impose a flexed width/height override
    virtual int overrideSize() const { return 0; }
    virtual int overrideWidth() const { return 0; }
    virtual int overrideHeight() const { return 0; }
    virtual void setOverrideSize(int /*overrideSize*/) { }

    // relative to parent node
    virtual void setPos(int /*xPos*/, int /*yPos*/) { }
    virtual void setWidth(int /*width*/) { }
    virtual void setHeight(int /*height*/) { }

    virtual int xPos() const { return 0; }
    virtual int yPos() const { return 0; }

    // calculate client position of box
    virtual bool absolutePosition(int& x, int& y, bool fixed = false) const;

    // This function is used to deal with the extra top space that can occur in table cells (called borderTopExtra).
    // The children of the cell do not factor this space in, so we have to add it in.  Any code that wants to
    // accurately deal with the contents of a cell must call this function instad of absolutePosition.
    bool absolutePositionForContent(int& xPos, int& yPos, bool fixed = false) const
    {
        bool result = absolutePosition(xPos, yPos, fixed);
        yPos += borderTopExtra();
        return result;
    }

    // width and height are without margins but include paddings and borders
    virtual int width() const { return 0; }
    virtual int height() const { return 0; }

    virtual IntRect borderBox() const { return IntRect(0, 0, width(), height()); }
    IntRect absoluteOutlineBox() const;

    // The height of a block when you include normal flow overflow spillage out of the bottom
    // of the block (e.g., a <div style="height:25px"> that has a 100px tall image inside
    // it would have an overflow height of borderTop() + paddingTop() + 100px.
    virtual int overflowHeight(bool /*includeInterior*/ = true) const { return height(); }
    virtual int overflowWidth(bool /*includeInterior*/ = true) const { return width(); }
    virtual void setOverflowHeight(int) { }
    virtual void setOverflowWidth(int) { }
    virtual int overflowLeft(bool /*includeInterior*/ = true) const { return 0; }
    virtual int overflowTop(bool /*includeInterior*/ = true) const { return 0; }
    virtual IntRect overflowRect(bool /*includeInterior*/ = true) const { return borderBox(); }

    // IE extensions. Used to calculate offsetWidth/Height.  Overridden by inlines (RenderFlow)
    // to return the remaining width on a given line (and the height of a single line).
    virtual int offsetWidth() const { return width(); }
    virtual int offsetHeight() const { return height() + borderTopExtra() + borderBottomExtra(); }

    // IE extensions.  Also supported by Gecko.  We override in render flow to get the
    // left and top correct. -dwh
    virtual int offsetLeft() const;
    virtual int offsetTop() const;
    virtual RenderObject* offsetParent() const;

    // More IE extensions.  clientWidth and clientHeight represent the interior of an object
    // excluding border and scrollbar.  clientLeft/Top are just the borderLeftWidth and borderTopWidth.
    int clientLeft() const { return borderLeft(); }
    int clientTop() const { return borderTop(); }
    int clientWidth() const;
    int clientHeight() const;

    // scrollWidth/scrollHeight will be the same as clientWidth/clientHeight unless the
    // object has overflow:hidden/scroll/auto specified and also has overflow.
    // scrollLeft/Top return the current scroll position.  These methods are virtual so that objects like
    // textareas can scroll shadow content (but pretend that they are the objects that are
    // scrolling).
    virtual int scrollLeft() const;
    virtual int scrollTop() const;
    virtual int scrollWidth() const;
    virtual int scrollHeight() const;
    virtual void setScrollLeft(int);
    virtual void setScrollTop(int);

    virtual bool scroll(ScrollDirection, ScrollGranularity, float multiplier = 1.0f);
    virtual bool shouldAutoscroll() const;
    virtual void autoscroll();
    virtual void stopAutoscroll() { }
    virtual bool isScrollable() const;

    // The following seven functions are used to implement collapsing margins.
    // All objects know their maximal positive and negative margins.  The
    // formula for computing a collapsed margin is |maxPosMargin|-|maxNegmargin|.
    // For a non-collapsing, e.g., a leaf element, this formula will simply return
    // the margin of the element.  Blocks override the maxTopMargin and maxBottomMargin
    // methods.
    virtual bool isSelfCollapsingBlock() const { return false; }
    virtual int collapsedMarginTop() const { return maxTopMargin(true) - maxTopMargin(false); }
    virtual int collapsedMarginBottom() const { return maxBottomMargin(true) - maxBottomMargin(false); }
    virtual bool isTopMarginQuirk() const { return false; }
    virtual bool isBottomMarginQuirk() const { return false; }

    virtual int maxTopMargin(bool positive) const;
    virtual int maxBottomMargin(bool positive) const;

    virtual int marginTop() const { return 0; }
    virtual int marginBottom() const { return 0; }
    virtual int marginLeft() const { return 0; }
    virtual int marginRight() const { return 0; }

    // Virtual since table cells override
    virtual int paddingTop() const;
    virtual int paddingBottom() const;
    virtual int paddingLeft() const;
    virtual int paddingRight() const;

    virtual int borderTop() const { return style()->borderTopWidth(); }
    virtual int borderBottom() const { return style()->borderBottomWidth(); }
    virtual int borderTopExtra() const { return 0; }
    virtual int borderBottomExtra() const { return 0; }
    virtual int borderLeft() const { return style()->borderLeftWidth(); }
    virtual int borderRight() const { return style()->borderRightWidth(); }

    virtual void addLineBoxRects(Vector<IntRect>&, unsigned startOffset = 0, unsigned endOffset = UINT_MAX, bool useSelectionHeight = false);

    virtual void absoluteRects(Vector<IntRect>&, int tx, int ty, bool topLevel = true);
    IntRect absoluteBoundingBoxRect();

    // the rect that will be painted if this object is passed as the paintingRoot
    IntRect paintingRootRect(IntRect& topLevelRect);

    void addPDFURLRect(GraphicsContext*, const IntRect&);

    virtual void addFocusRingRects(GraphicsContext*, int tx, int ty);

    virtual int minPrefWidth() const { return 0; }
    virtual int maxPrefWidth() const { return 0; }

    RenderStyle* style() const { return m_style.get(); }
    RenderStyle* firstLineStyle() const;
    RenderStyle* style(bool firstLine) const { return firstLine ? firstLineStyle() : style(); }

    void getTextDecorationColors(int decorations, Color& underline, Color& overline,
                                 Color& linethrough, bool quirksMode = false);

    enum BorderSide {
        BSTop,
        BSBottom,
        BSLeft,
        BSRight
    };

    void drawBorderArc(GraphicsContext*, int x, int y, float thickness, IntSize radius, int angleStart,
                       int angleSpan, BorderSide, Color, const Color& textcolor, EBorderStyle, bool firstCorner);
    void drawBorder(GraphicsContext*, int x1, int y1, int x2, int y2, BorderSide,
                    Color, const Color& textcolor, EBorderStyle, int adjbw1, int adjbw2);

    // Repaint the entire object.  Called when, e.g., the color of a border changes, or when a border
    // style changes.
    void repaint(bool immediate = false);

    // Repaint a specific subrectangle within a given object.  The rect |r| is in the object's coordinate space.
    void repaintRectangle(const IntRect&, bool immediate = false);

    // Repaint only if our old bounds and new bounds are different.
    bool repaintAfterLayoutIfNeeded(const IntRect& oldBounds, const IntRect& oldOutlineBox);

    // Repaint only if the object moved.
    virtual void repaintDuringLayoutIfMoved(const IntRect& rect);

    // Called to repaint a block's floats.
    virtual void repaintOverhangingFloats(bool paintAllDescendants = false);

    bool checkForRepaintDuringLayout() const;

    // Returns the rect that should be repainted whenever this object changes.  The rect is in the view's
    // coordinate space.  This method deals with outlines and overflow.
    virtual IntRect absoluteClippedOverflowRect();

    IntRect getAbsoluteRepaintRectWithOutline(int ow);

    // Given a rect in the object's coordinate space, this method converts the rectangle to the view's
    // coordinate space.
    virtual void computeAbsoluteRepaintRect(IntRect&, bool fixed = false);

    virtual unsigned int length() const { return 1; }

    bool isFloatingOrPositioned() const { return (isFloating() || isPositioned()); }
    virtual bool containsFloats() { return false; }
    virtual bool containsFloat(RenderObject*) { return false; }
    virtual bool hasOverhangingFloats() { return false; }
    virtual bool expandsToEncloseOverhangingFloats() const { return isFloating() && style()->height().isAuto(); }

    virtual void removePositionedObjects(RenderBlock*) { }

    virtual bool avoidsFloats() const;
    bool shrinkToAvoidFloats() const;

    // positioning of inline children (bidi)
    virtual void position(InlineBox*) { }

    bool isTransparent() const { return style()->opacity() < 1.0f; }
    float opacity() const { return style()->opacity(); }

    bool hasReflection() const { return m_hasReflection; }
    IntRect reflectionBox() const;
    int reflectionOffset() const;

    // Applied as a "slop" to dirty rect checks during the outline painting phase's dirty-rect checks.
    int maximalOutlineSize(PaintPhase) const;

    enum SelectionState {
        SelectionNone, // The object is not selected.
        SelectionStart, // The object either contains the start of a selection run or is the start of a run
        SelectionInside, // The object is fully encompassed by a selection run
        SelectionEnd, // The object either contains the end of a selection run or is the end of a run
        SelectionBoth // The object contains an entire run or is the sole selected object in that run
    };

    // The current selection state for an object.  For blocks, the state refers to the state of the leaf
    // descendants (as described above in the SelectionState enum declaration).
    virtual SelectionState selectionState() const { return SelectionNone; }

    // Sets the selection state for an object.
    virtual void setSelectionState(SelectionState state) { if (parent()) parent()->setSelectionState(state); }

    // A single rectangle that encompasses all of the selected objects within this object.  Used to determine the tightest
    // possible bounding box for the selection.
    virtual IntRect selectionRect(bool) { return IntRect(); }

    // Whether or not an object can be part of the leaf elements of the selection.
    virtual bool canBeSelectionLeaf() const { return false; }

    // Whether or not a block has selected children.
    virtual bool hasSelectedChildren() const { return false; }

    // Obtains the selection colors that should be used when painting a selection.
    Color selectionBackgroundColor() const;
    Color selectionForegroundColor() const;

    // Whether or not a given block needs to paint selection gaps.
    virtual bool shouldPaintSelectionGaps() const { return false; }

    // This struct is used when the selection changes to cache the old and new state of the selection for each RenderObject.
    struct SelectionInfo: public WTF::FastAllocBase {
        SelectionInfo()
            : m_object(0)
            , m_state(SelectionNone)
        {
        }

        SelectionInfo(RenderObject* o, bool clipToVisibleContent)
            : m_object(o)
            , m_rect(o->needsLayout() ? IntRect() : o->selectionRect(clipToVisibleContent))
            , m_state(o->selectionState())
        {
        }

        RenderObject* object() const { return m_object; }
        IntRect rect() const { return m_rect; }
        SelectionState state() const { return m_state; }

        RenderObject* m_object;
        IntRect m_rect;
        SelectionState m_state;
    };

    Node* draggableNode(bool dhtmlOK, bool uaOK, int x, int y, bool& dhtmlWillDrag) const;

    /**
     * Returns the content coordinates of the caret within this render object.
     * @param offset zero-based offset determining position within the render object.
     * @param override @p true if input overrides existing characters,
     * @p false if it inserts them. The width of the caret depends on this one.
     * @param extraWidthToEndOfLine optional out arg to give extra width to end of line -
     * useful for character range rect computations
     */
    virtual IntRect caretRect(InlineBox*, int caretOffset, int* extraWidthToEndOfLine = 0);

    virtual int lowestPosition(bool /*includeOverflowInterior*/ = true, bool /*includeSelf*/ = true) const { return 0; }
    virtual int rightmostPosition(bool /*includeOverflowInterior*/ = true, bool /*includeSelf*/ = true) const { return 0; }
    virtual int leftmostPosition(bool /*includeOverflowInterior*/ = true, bool /*includeSelf*/ = true) const { return 0; }

    virtual void calcVerticalMargins() { }
    void removeFromObjectLists();

    // When performing a global document tear-down, the renderer of the document is cleared.  We use this
    // as a hook to detect the case of document destruction and don't waste time doing unnecessary work.
    bool documentBeingDestroyed() const;

    virtual void destroy();

    // Virtual function helpers for CSS3 Flexible Box Layout
    virtual bool isFlexibleBox() const { return false; }
    virtual bool isFlexingChildren() const { return false; }
    virtual bool isStretchingChildren() const { return false; }

    // Convenience, to avoid repeating the code to dig down to get this.
    UChar backslashAsCurrencySymbol() const;

    virtual int caretMinOffset() const;
    virtual int caretMaxOffset() const;
    virtual unsigned caretMaxRenderedOffset() const;

    virtual int previousOffset(int current) const;
    virtual int nextOffset(int current) const;

    virtual void imageChanged(CachedImage* image);
    virtual void imageChanged(WrappedImagePtr data) { };
    virtual bool willRenderImage(CachedImage*);

    virtual void selectionStartEnd(int& spos, int& epos) const;

    RenderObject* paintingRootForChildren(PaintInfo& paintInfo) const
    {
        // if we're the painting root, kids draw normally, and see root of 0
        return (!paintInfo.paintingRoot || paintInfo.paintingRoot == this) ? 0 : paintInfo.paintingRoot;
    }

    bool shouldPaintWithinRoot(PaintInfo& paintInfo) const
    {
        return !paintInfo.paintingRoot || paintInfo.paintingRoot == this;
    }

    bool hasOverrideSize() const { return m_hasOverrideSize; }
    void setHasOverrideSize(bool b) { m_hasOverrideSize = b; }
    
    void remove() { if (parent()) parent()->removeChild(this); }

    void invalidateVerticalPosition() { m_verticalPosition = PositionUndefined; }
    
    virtual void removeLeftoverAnonymousBlock(RenderBlock* child);
    
    virtual void capsLockStateMayHaveChanged() { }

    AnimationController* animation() const;

protected:
    // Overrides should call the superclass at the end
    virtual void styleWillChange(RenderStyle::Diff, const RenderStyle* newStyle);
    // Overrides should call the superclass at the start
    virtual void styleDidChange(RenderStyle::Diff, const RenderStyle* oldStyle);
    
    virtual void printBoxDecorations(GraphicsContext*, int /*x*/, int /*y*/, int /*w*/, int /*h*/, int /*tx*/, int /*ty*/) { }

    virtual IntRect viewRect() const;

    int getVerticalPosition(bool firstLine) const;

    void adjustRectForOutlineAndShadow(IntRect&) const;

    void arenaDelete(RenderArena*, void* objectBase);

private:
    RefPtr<RenderStyle> m_style;

    Node* m_node;

    RenderObject* m_parent;
    RenderObject* m_previous;
    RenderObject* m_next;

#ifndef NDEBUG
    bool m_hasAXObject;
#endif
    mutable int m_verticalPosition;

    bool m_needsLayout               : 1;
    bool m_needsPositionedMovementLayout :1;
    bool m_normalChildNeedsLayout    : 1;
    bool m_posChildNeedsLayout       : 1;
    bool m_prefWidthsDirty           : 1;
    bool m_floating                  : 1;

    bool m_positioned                : 1;
    bool m_relPositioned             : 1;
    bool m_paintBackground           : 1; // if the box has something to paint in the
                                          // background painting phase (background, border, etc)

    bool m_isAnonymous               : 1;
    bool m_isText                    : 1;
    bool m_inline                    : 1;
    bool m_replaced                  : 1;
    bool m_isDragging                : 1;

    bool m_hasLayer                  : 1;
    bool m_hasOverflowClip           : 1;
    bool m_hasTransform              : 1;
    bool m_hasReflection             : 1;

    bool m_hasOverrideSize           : 1;
    
public:
    bool m_hasCounterNodeMap         : 1;
    bool m_everHadLayout             : 1;

private:
    // Store state between styleWillChange and styleDidChange
    static bool s_affectsParentBlock;
};

} // namespace WebCore

#ifndef NDEBUG
// Outside the WebCore namespace for ease of invocation from gdb.
void showTree(const WebCore::RenderObject*);
#endif

#endif // RenderObject_h
