/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
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
 */

/*
* This file was modified by Electronic Arts Inc Copyright � 2009
*/

#include "config.h"
#include "CSSStyleSelector.h"

#include "CSSBorderImageValue.h"
#include "CSSCursorImageValue.h"
#include "CSSFontFace.h"
#include "CSSFontFaceRule.h"
#include "CSSFontFaceSource.h"
#include "CSSImportRule.h"
#include "CSSMediaRule.h"
#include "CSSParser.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSProperty.h"
#include "CSSPropertyNames.h"
#include "CSSReflectValue.h"
#include "CSSRuleList.h"
#include "CSSSelector.h"
#include "CSSStyleRule.h"
#include "CSSStyleSheet.h"
#include "CSSTimingFunctionValue.h"
#include "CSSTransformValue.h"
#include "CSSValueList.h"
#include "CSSVariableDependentValue.h"
#include "CSSVariablesDeclaration.h"
#include "CSSVariablesRule.h"
#include "CachedImage.h"
#include "Counter.h"
#include "FontFamilyValue.h"
#include "FontValue.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLDocument.h"
#include "HTMLElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "MediaList.h"
#include "MediaQueryEvaluator.h"
#include "Page.h"
#include "PageGroup.h"
#include "Pair.h"
#include "Rect.h"
#include "RenderTheme.h"
#include "SelectionController.h"
#include "Settings.h"
#include "ShadowValue.h"
#include "StyleSheetList.h"
#include "Text.h"
#include "UserAgentStyleSheets.h"
#include "XMLNames.h"
#include "loader.h"
#include <wtf/Vector.h>

#if ENABLE(DASHBOARD_SUPPORT)
#include "DashboardRegion.h"
#endif

#if ENABLE(SVG)
#include "XLinkNames.h"
#include "SVGNames.h"
#endif

using namespace std;

namespace WebCore {

using namespace HTMLNames;

// #define STYLE_SHARING_STATS 1

#define HANDLE_INHERIT(prop, Prop) \
if (isInherit) { \
    m_style->set##Prop(m_parentStyle->prop()); \
    return; \
}

#define HANDLE_INHERIT_AND_INITIAL(prop, Prop) \
HANDLE_INHERIT(prop, Prop) \
if (isInitial) { \
    m_style->set##Prop(RenderStyle::initial##Prop()); \
    return; \
}

#define HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(prop, Prop, Value) \
HANDLE_INHERIT(prop, Prop) \
if (isInitial) { \
    m_style->set##Prop(RenderStyle::initial##Value());\
    return;\
}

#define HANDLE_FILL_LAYER_INHERIT_AND_INITIAL(layerType, LayerType, prop, Prop) \
if (isInherit) { \
    FillLayer* currChild = m_style->access##LayerType##Layers(); \
    FillLayer* prevChild = 0; \
    const FillLayer* currParent = m_parentStyle->layerType##Layers(); \
    while (currParent && currParent->is##Prop##Set()) { \
        if (!currChild) { \
            /* Need to make a new layer.*/ \
            currChild = new FillLayer(LayerType##FillLayer); \
            prevChild->setNext(currChild); \
        } \
        currChild->set##Prop(currParent->prop()); \
        prevChild = currChild; \
        currChild = prevChild->next(); \
        currParent = currParent->next(); \
    } \
    \
    while (currChild) { \
        /* Reset any remaining layers to not have the property set. */ \
        currChild->clear##Prop(); \
        currChild = currChild->next(); \
    } \
} else if (isInitial) { \
    FillLayer* currChild = m_style->access##LayerType##Layers(); \
    currChild->set##Prop(FillLayer::initialFill##Prop(LayerType##FillLayer)); \
    for (currChild = currChild->next(); currChild; currChild = currChild->next()) \
        currChild->clear##Prop(); \
}

#define HANDLE_FILL_LAYER_VALUE(layerType, LayerType, prop, Prop, value) { \
HANDLE_FILL_LAYER_INHERIT_AND_INITIAL(layerType, LayerType, prop, Prop) \
if (isInherit || isInitial) \
    return; \
FillLayer* currChild = m_style->access##LayerType##Layers(); \
FillLayer* prevChild = 0; \
if (value->isValueList()) { \
    /* Walk each value and put it into a layer, creating new layers as needed. */ \
    CSSValueList* valueList = static_cast<CSSValueList*>(value); \
    for (unsigned int i = 0; i < valueList->length(); i++) { \
        if (!currChild) { \
            /* Need to make a new layer to hold this value */ \
            currChild = new FillLayer(LayerType##FillLayer); \
            prevChild->setNext(currChild); \
        } \
        mapFill##Prop(currChild, valueList->itemWithoutBoundsCheck(i)); \
        prevChild = currChild; \
        currChild = currChild->next(); \
    } \
} else { \
    mapFill##Prop(currChild, value); \
    currChild = currChild->next(); \
} \
while (currChild) { \
    /* Reset all remaining layers to not have the property set. */ \
    currChild->clear##Prop(); \
    currChild = currChild->next(); \
} }

#define HANDLE_BACKGROUND_INHERIT_AND_INITIAL(prop, Prop) \
HANDLE_FILL_LAYER_INHERIT_AND_INITIAL(background, Background, prop, Prop)

#define HANDLE_BACKGROUND_VALUE(prop, Prop, value) \
HANDLE_FILL_LAYER_VALUE(background, Background, prop, Prop, value)

#define HANDLE_MASK_INHERIT_AND_INITIAL(prop, Prop) \
HANDLE_FILL_LAYER_INHERIT_AND_INITIAL(mask, Mask, prop, Prop)

#define HANDLE_MASK_VALUE(prop, Prop, value) \
HANDLE_FILL_LAYER_VALUE(mask, Mask, prop, Prop, value)

#define HANDLE_TRANSITION_INHERIT_AND_INITIAL(prop, Prop) \
if (isInherit) { \
    Transition* currChild = m_style->accessTransitions(); \
    Transition* prevChild = 0; \
    const Transition* currParent = m_parentStyle->transitions(); \
    while (currParent && currParent->is##Prop##Set()) { \
        if (!currChild) { \
            /* Need to make a new layer.*/ \
            currChild = new Transition(); \
            prevChild->setNext(currChild); \
        } \
        currChild->set##Prop(currParent->prop()); \
        prevChild = currChild; \
        currChild = prevChild->next(); \
        currParent = currParent->next(); \
    } \
    \
    while (currChild) { \
        /* Reset any remaining layers to not have the property set. */ \
        currChild->clear##Prop(); \
        currChild = currChild->next(); \
    } \
} else if (isInitial) { \
    Transition* currChild = m_style->accessTransitions(); \
    currChild->set##Prop(RenderStyle::initialTransition##Prop()); \
    for (currChild = currChild->next(); currChild; currChild = currChild->next()) \
        currChild->clear##Prop(); \
}

#define HANDLE_TRANSITION_VALUE(prop, Prop, value) { \
HANDLE_TRANSITION_INHERIT_AND_INITIAL(prop, Prop) \
if (isInherit || isInitial) \
    return; \
Transition* currChild = m_style->accessTransitions(); \
Transition* prevChild = 0; \
if (value->isValueList()) { \
    /* Walk each value and put it into a layer, creating new layers as needed. */ \
    CSSValueList* valueList = static_cast<CSSValueList*>(value); \
    for (unsigned int i = 0; i < valueList->length(); i++) { \
        if (!currChild) { \
            /* Need to make a new layer to hold this value */ \
            currChild = new Transition(); \
            prevChild->setNext(currChild); \
        } \
        mapTransition##Prop(currChild, valueList->itemWithoutBoundsCheck(i)); \
        prevChild = currChild; \
        currChild = currChild->next(); \
    } \
} else { \
    mapTransition##Prop(currChild, value); \
    currChild = currChild->next(); \
} \
while (currChild) { \
    /* Reset all remaining layers to not have the property set. */ \
    currChild->clear##Prop(); \
    currChild = currChild->next(); \
} }

#define HANDLE_INHERIT_COND(propID, prop, Prop) \
if (id == propID) { \
    m_style->set##Prop(m_parentStyle->prop()); \
    return; \
}
    
#define HANDLE_INHERIT_COND_WITH_BACKUP(propID, prop, propAlt, Prop) \
if (id == propID) { \
    if (m_parentStyle->prop().isValid()) \
        m_style->set##Prop(m_parentStyle->prop()); \
    else \
        m_style->set##Prop(m_parentStyle->propAlt()); \
    return; \
}

#define HANDLE_INITIAL_COND(propID, Prop) \
if (id == propID) { \
    m_style->set##Prop(RenderStyle::initial##Prop()); \
    return; \
}

#define HANDLE_INITIAL_COND_WITH_VALUE(propID, Prop, Value) \
if (id == propID) { \
    m_style->set##Prop(RenderStyle::initial##Value()); \
    return; \
}
#include <wtf/FastAllocBase.h>

class CSSRuleSet: public WTF::FastAllocBase {
public:
    CSSRuleSet();
    ~CSSRuleSet();
    
    typedef HashMap<AtomicStringImpl*, CSSRuleDataList*> AtomRuleMap;
    
    void addRulesFromSheet(CSSStyleSheet*, const MediaQueryEvaluator&, CSSStyleSelector* = 0);
    
    void addRule(CSSStyleRule* rule, CSSSelector* sel);
    void addToRuleSet(AtomicStringImpl* key, AtomRuleMap& map,
                      CSSStyleRule* rule, CSSSelector* sel);
    
    CSSRuleDataList* getIDRules(AtomicStringImpl* key) { return m_idRules.get(key); }
    CSSRuleDataList* getClassRules(AtomicStringImpl* key) { return m_classRules.get(key); }
    CSSRuleDataList* getTagRules(AtomicStringImpl* key) { return m_tagRules.get(key); }
    CSSRuleDataList* getUniversalRules() { return m_universalRules; }
    
public:
    AtomRuleMap m_idRules;
    AtomRuleMap m_classRules;
    AtomRuleMap m_tagRules;
    CSSRuleDataList* m_universalRules;
    unsigned m_ruleCount;
};

static CSSRuleSet* defaultStyle = 0;
static CSSRuleSet* defaultQuirksStyle = 0;
static CSSRuleSet* defaultPrintStyle = 0;
static CSSRuleSet* defaultViewSourceStyle = 0;
static CSSStyleSheet* defaultSheet = 0;

RenderStyle* CSSStyleSelector::s_styleNotYetAvailable = 0;

static CSSStyleSheet* quirksSheet = 0;
static CSSStyleSheet* viewSourceSheet = 0;
// static CSSStyleSheet* simpleDefaultStyleSheet =0;

// 3/27/09 CSidhall - Moved statics out of functions so they can be accessed for leak fix
static HashSet<AtomicStringImpl*>* sHtmlCaseInsensitiveAttributesSet = NULL;
static MediaQueryEvaluator* spStaticPrintEval = NULL;
static MediaQueryEvaluator* spStaticScreenEval = NULL;
static PseudoState pseudoState;

static void loadDefaultStyle();

static const MediaQueryEvaluator& screenEval()
{
    if(!spStaticScreenEval)
        spStaticScreenEval = new MediaQueryEvaluator("screen");
    return *spStaticScreenEval;
}

static const MediaQueryEvaluator& printEval()
{
    if(!spStaticPrintEval)
        spStaticPrintEval = new MediaQueryEvaluator("print");
    return *spStaticPrintEval;
}

CSSStyleSelector::CSSStyleSelector(Document* doc, const String& userStyleSheet, StyleSheetList* styleSheets, CSSStyleSheet* mappedElementSheet, bool strictParsing, bool matchAuthorAndUserStyles)
    : m_backgroundData(BackgroundFillLayer)
    , m_checker(doc, strictParsing, false)
    , m_fontSelector(CSSFontSelector::create(doc))
{
    init();

    m_matchAuthorAndUserStyles = matchAuthorAndUserStyles;

    if (!defaultStyle)
        loadDefaultStyle();

    m_userStyle = 0;

    // construct document root element default style. this is needed
    // to evaluate media queries that contain relative constraints, like "screen and (max-width: 10em)"
    // This is here instead of constructor, because when constructor is run,
    // document doesn't have documentElement
    // NOTE: this assumes that element that gets passed to styleForElement -call
    // is always from the document that owns the style selector
    FrameView* view = doc->view();
    if (view)
        m_medium = new MediaQueryEvaluator(view->mediaType());
    else
        m_medium = new MediaQueryEvaluator("all");

    Element* root = doc->documentElement();

    if (root)
        m_rootDefaultStyle = styleForElement(root, 0, false, true); // dont ref, because the RenderStyle is allocated from global heap

    if (m_rootDefaultStyle && view) {
        delete m_medium;
        m_medium = new MediaQueryEvaluator(view->mediaType(), view->frame(), m_rootDefaultStyle.get());
    }

    // FIXME: This sucks! The user sheet is reparsed every time!
    if (!userStyleSheet.isEmpty()) {
        m_userSheet = CSSStyleSheet::create(doc);
        m_userSheet->parseString(userStyleSheet, strictParsing);

        m_userStyle = new CSSRuleSet();
        m_userStyle->addRulesFromSheet(m_userSheet.get(), *m_medium, this);
    }

    // add stylesheets from document
    m_authorStyle = new CSSRuleSet();
    
    // Add rules from elments like SVG's <font-face>
    if (mappedElementSheet)
        m_authorStyle->addRulesFromSheet(mappedElementSheet, *m_medium, this);

    unsigned length = styleSheets->length();
    for (unsigned i = 0; i < length; i++) {
        StyleSheet* sheet = styleSheets->item(i);
        if (sheet->isCSSStyleSheet() && !sheet->disabled())
            m_authorStyle->addRulesFromSheet(static_cast<CSSStyleSheet*>(sheet), *m_medium, this);
    }
}

void CSSStyleSelector::init()
{
    m_element = 0;
    m_matchedDecls.clear();
    m_ruleList = 0;
    m_rootDefaultStyle = 0;
    m_medium = 0;
}

CSSStyleSelector::~CSSStyleSelector()
{
    delete m_medium;
    delete m_authorStyle;
    delete m_userStyle;
    deleteAllValues(m_viewportDependentMediaQueryResults);
}

// Added by Paul Pedriana in order to prevent memory leaks:
CSSStyleSheet* gUASheets[6];
int            gUASheetsCount = 0;

static CSSStyleSheet* parseUASheet(const char* characters, unsigned size)
{
    ASSERT(gUASheetsCount < (int)(sizeof(gUASheets)/sizeof(gUASheets[0])));

    CSSStyleSheet* sheet = CSSStyleSheet::create().releaseRef();    // The original WebKit code did this to lose the ref count. Our global gUASheets saves the pointer so we can later manually release it.
    sheet->parseString(String(characters, size));                   // Ideally the original WebKit code would be fixed to not intentionally leak memory like that.
    gUASheets[gUASheetsCount++] = sheet;
    return sheet;
}

static void loadDefaultStyle()
{
    ASSERT(!defaultStyle);

    defaultStyle = new CSSRuleSet;
    defaultPrintStyle = new CSSRuleSet;
    defaultQuirksStyle = new CSSRuleSet;
    defaultViewSourceStyle = new CSSRuleSet;

    // Strict-mode rules.
    CSSStyleSheet* defaultSheet = parseUASheet(html4UserAgentStyleSheet, sizeof(html4UserAgentStyleSheet));
    RenderTheme::adjustDefaultStyleSheet(defaultSheet);
    defaultStyle->addRulesFromSheet(defaultSheet, screenEval());
    defaultPrintStyle->addRulesFromSheet(defaultSheet, printEval());

    // Quirks-mode rules.
    CSSStyleSheet* sheet = parseUASheet(quirksUserAgentStyleSheet, sizeof(quirksUserAgentStyleSheet));
    defaultQuirksStyle->addRulesFromSheet(sheet, screenEval());
    
    // View source rules.
    sheet = parseUASheet(sourceUserAgentStyleSheet, sizeof(sourceUserAgentStyleSheet));
    defaultViewSourceStyle->addRulesFromSheet(sheet, screenEval());
}

void CSSStyleSelector::addMatchedDeclaration(CSSMutableStyleDeclaration* decl)
{
    if (!decl->hasVariableDependentValue()) {
        m_matchedDecls.append(decl);
        return;
    }

    // See if we have already resolved the variables in this declaration.
    CSSMutableStyleDeclaration* resolvedDecl = m_resolvedVariablesDeclarations.get(decl).get();
    if (resolvedDecl) {
        m_matchedDecls.append(resolvedDecl);
        return;
    }

    // If this declaration has any variables in it, then we need to make a cloned
    // declaration with as many variables resolved as possible for this style selector's media.
    RefPtr<CSSMutableStyleDeclaration> newDecl = CSSMutableStyleDeclaration::create(decl->parentRule());
    *newDecl = *decl;
    m_matchedDecls.append(newDecl.get());
    m_resolvedVariablesDeclarations.set(decl, newDecl);

    // Now iterate over the properties in the original declaration.  As we resolve variables we'll end up
    // mutating the new declaration (possibly expanding shorthands).  The new declaration has no m_node
    // though, so it can't mistakenly call setChanged on anything.
    DeprecatedValueListConstIterator<CSSProperty> end;
    for (DeprecatedValueListConstIterator<CSSProperty> it = decl->valuesIterator(); it != end; ++it) {
        const CSSProperty& current = *it;
        if (!current.value()->isVariableDependentValue())
            continue;
        CSSValueList* valueList = static_cast<CSSVariableDependentValue*>(current.value())->valueList();
        if (!valueList)
            continue;
        CSSParserValueList resolvedValueList;
        unsigned s = valueList->length();
        bool fullyResolved = true;
        for (unsigned i = 0; i < s; ++i) {
            CSSValue* val = valueList->item(i);
            CSSPrimitiveValue* primitiveValue = val->isPrimitiveValue() ? static_cast<CSSPrimitiveValue*>(val) : 0;
            if (primitiveValue && primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_PARSER_VARIABLE) {
                CSSVariablesRule* rule = m_variablesMap.get(primitiveValue->getStringValue());
                if (!rule || !rule->variables()) {
                    fullyResolved = false;
                    break;
                }
                CSSValueList* resolvedVariable = rule->variables()->getParsedVariable(primitiveValue->getStringValue());
                if (!resolvedVariable) {
                    fullyResolved = false;
                    break;
                }
                unsigned valueSize = resolvedVariable->length();
                for (unsigned j = 0; j < valueSize; ++j)
                    resolvedValueList.addValue(resolvedVariable->item(j)->parserValue());
            } else
                resolvedValueList.addValue(val->parserValue());
        }
        
        if (!fullyResolved)
            continue;

        // We now have a fully resolved new value list.  We want the parser to use this value list
        // and parse our new declaration.
        CSSParser(m_checker.m_strictParsing).parsePropertyWithResolvedVariables(current.id(), current.isImportant(), newDecl.get(), &resolvedValueList);
    }
}

void CSSStyleSelector::matchRules(CSSRuleSet* rules, int& firstRuleIndex, int& lastRuleIndex)
{
    m_matchedRules.clear();

    if (!rules || !m_element)
        return;
    
    // We need to collect the rules for id, class, tag, and everything else into a buffer and
    // then sort the buffer.
    if (m_element->hasID())
        matchRulesForList(rules->getIDRules(m_element->getIDAttribute().impl()), firstRuleIndex, lastRuleIndex);
    if (m_element->hasClass()) {
        ASSERT(m_styledElement);
        const ClassNames& classNames = m_styledElement->classNames();
        size_t size = classNames.size();
        for (size_t i = 0; i < size; ++i)
            matchRulesForList(rules->getClassRules(classNames[i].impl()), firstRuleIndex, lastRuleIndex);
    }
    matchRulesForList(rules->getTagRules(m_element->localName().impl()), firstRuleIndex, lastRuleIndex);
    matchRulesForList(rules->getUniversalRules(), firstRuleIndex, lastRuleIndex);
    
    // If we didn't match any rules, we're done.
    if (m_matchedRules.isEmpty())
        return;
    
    // Sort the set of matched rules.
    sortMatchedRules(0, m_matchedRules.size());
    
    // Now transfer the set of matched rules over to our list of decls.
    if (!m_checker.m_collectRulesOnly) {
        for (unsigned i = 0; i < m_matchedRules.size(); i++)
            addMatchedDeclaration(m_matchedRules[i]->rule()->declaration());
    } else {
        for (unsigned i = 0; i < m_matchedRules.size(); i++) {
            if (!m_ruleList)
                m_ruleList = CSSRuleList::create();
            m_ruleList->append(m_matchedRules[i]->rule());
        }
    }
}

void CSSStyleSelector::matchRulesForList(CSSRuleDataList* rules, int& firstRuleIndex, int& lastRuleIndex)
{
    if (!rules)
        return;

    for (CSSRuleData* d = rules->first(); d; d = d->next()) {
        CSSStyleRule* rule = d->rule();
        const AtomicString& localName = m_element->localName();
        const AtomicString& selectorLocalName = d->selector()->m_tag.localName();
        if ((localName == selectorLocalName || selectorLocalName == starAtom) && checkSelector(d->selector())) {
            // If the rule has no properties to apply, then ignore it.
            CSSMutableStyleDeclaration* decl = rule->declaration();
            if (!decl || !decl->length())
                continue;
            
            // If we're matching normal rules, set a pseudo bit if 
            // we really just matched a pseudo-element.
            if (m_dynamicPseudo != RenderStyle::NOPSEUDO && m_checker.m_pseudoStyle == RenderStyle::NOPSEUDO) {
                if (m_checker.m_collectRulesOnly)
                    return;
                if (m_dynamicPseudo < RenderStyle::FIRST_INTERNAL_PSEUDOID)
                {
                    m_style->setHasPseudoStyle(m_dynamicPseudo);
                }
            } else {
                // Update our first/last rule indices in the matched rules array.
                lastRuleIndex = m_matchedDecls.size() + m_matchedRules.size();
                if (firstRuleIndex == -1)
                    firstRuleIndex = lastRuleIndex;

                // Add this rule to our list of matched rules.
                addMatchedRule(d);
            }
        }
    }
}

bool operator >(CSSRuleData& r1, CSSRuleData& r2)
{
    int spec1 = r1.selector()->specificity();
    int spec2 = r2.selector()->specificity();
    return (spec1 == spec2) ? r1.position() > r2.position() : spec1 > spec2; 
}
bool operator <=(CSSRuleData& r1, CSSRuleData& r2)
{
    return !(r1 > r2);
}

void CSSStyleSelector::sortMatchedRules(unsigned start, unsigned end)
{
    if (start >= end || (end - start == 1))
        return; // Sanity check.

    if (end - start <= 6) {
        // Apply a bubble sort for smaller lists.
        for (unsigned i = end - 1; i > start; i--) {
            bool sorted = true;
            for (unsigned j = start; j < i; j++) {
                CSSRuleData* elt = m_matchedRules[j];
                CSSRuleData* elt2 = m_matchedRules[j + 1];
                if (*elt > *elt2) {
                    sorted = false;
                    m_matchedRules[j] = elt2;
                    m_matchedRules[j + 1] = elt;
                }
            }
            if (sorted)
                return;
        }
        return;
    }

    // Peform a merge sort for larger lists.
    unsigned mid = (start + end) / 2;
    sortMatchedRules(start, mid);
    sortMatchedRules(mid, end);
    
    CSSRuleData* elt = m_matchedRules[mid - 1];
    CSSRuleData* elt2 = m_matchedRules[mid];
    
    // Handle the fast common case (of equal specificity).  The list may already
    // be completely sorted.
    if (*elt <= *elt2)
        return;
    
    // We have to merge sort.  Ensure our merge buffer is big enough to hold
    // all the items.
    Vector<CSSRuleData*> rulesMergeBuffer;
    rulesMergeBuffer.reserveCapacity(end - start); 

    unsigned i1 = start;
    unsigned i2 = mid;
    
    elt = m_matchedRules[i1];
    elt2 = m_matchedRules[i2];
    
    while (i1 < mid || i2 < end) {
        if (i1 < mid && (i2 == end || *elt <= *elt2)) {
            rulesMergeBuffer.append(elt);
            if (++i1 < mid)
                elt = m_matchedRules[i1];
        } else {
            rulesMergeBuffer.append(elt2);
            if (++i2 < end)
                elt2 = m_matchedRules[i2];
        }
    }
    
    for (unsigned i = start; i < end; i++)
        m_matchedRules[i] = rulesMergeBuffer[i - start];
}

void CSSStyleSelector::initElementAndPseudoState(Element* e)
{
    m_element = e;
    if (m_element && m_element->isStyledElement())
        m_styledElement = static_cast<StyledElement*>(m_element);
    else
        m_styledElement = 0;
    pseudoState = PseudoUnknown;
}

void CSSStyleSelector::initForStyleResolve(Element* e, RenderStyle* parentStyle, RenderStyle::PseudoId pseudoID)
{
    m_checker.m_pseudoStyle = pseudoID;

    m_parentNode = e->parentNode();

#if ENABLE(SVG)
    if (!m_parentNode && e->isSVGElement() && e->isShadowNode())
        m_parentNode = e->shadowParentNode();
#endif

    if (parentStyle)
        m_parentStyle = parentStyle;
    else
        m_parentStyle = m_parentNode ? m_parentNode->renderStyle() : 0;

    m_style = 0;

    m_matchedDecls.clear();

    m_ruleList = 0;

    m_fontDirty = false;
}

static inline const AtomicString* linkAttribute(Node* node)
{
    if (!node->isLink())
        return 0;

    ASSERT(node->isElementNode());
    Element* element = static_cast<Element*>(node);
    if (element->isHTMLElement())
        return &element->getAttribute(hrefAttr);
#if ENABLE(SVG)
    if (element->isSVGElement())
        return &element->getAttribute(XLinkNames::hrefAttr);
#endif
    return 0;
}

CSSStyleSelector::SelectorChecker::SelectorChecker(Document* document, bool strictParsing, bool collectRulesOnly)
    : m_document(document)
    , m_strictParsing(strictParsing)
    , m_collectRulesOnly(collectRulesOnly)
    , m_pseudoStyle(RenderStyle::NOPSEUDO)
    , m_documentIsHTML(document->isHTMLDocument())
{
}

PseudoState CSSStyleSelector::SelectorChecker::checkPseudoState(Element* element, bool checkVisited) const
{
    const AtomicString* attr = linkAttribute(element);
    if (!attr || attr->isNull())
        return PseudoNone;

    if (!checkVisited)
        return PseudoAnyLink;

    unsigned hash = m_document->visitedLinkHash(*attr);
    if (!hash)
        return PseudoLink;

    Frame* frame = m_document->frame();
    if (!frame)
        return PseudoLink;

    Page* page = frame->page();
    if (!page)
        return PseudoLink;

    m_linksCheckedForVisitedState.add(hash);
    return page->group().isLinkVisited(hash) ? PseudoVisited : PseudoLink;
}

bool CSSStyleSelector::SelectorChecker::checkSelector(CSSSelector* sel, Element* element) const
{
    pseudoState = PseudoUnknown;
    RenderStyle::PseudoId dynamicPseudo = RenderStyle::NOPSEUDO;

    return checkSelector(sel, element, 0, dynamicPseudo, true, false) == SelectorMatches;
}

// a helper function for parsing nth-arguments
static bool parseNth(const String& nth, int &a, int &b)
{
    if (nth.isEmpty())
        return false;
    a = 0;
    b = 0;
    if (nth == "odd") {
        a = 2;
        b = 1;
    } else if (nth == "even") {
        a = 2;
        b = 0;
    } else {
        int n = nth.find('n');
        if (n != -1) {
            if (nth[0] == '-') {
                if (n == 1)
                    a = -1; // -n == -1n
                else
                    a = nth.substring(0, n).toInt();
            } else if (!n)
                a = 1; // n == 1n
            else
                a = nth.substring(0, n).toInt();

            int p = nth.find('+', n);
            if (p != -1)
                b = nth.substring(p + 1, nth.length() - p - 1).toInt();
            else {
                p = nth.find('-', n);
                b = -nth.substring(p + 1, nth.length() - p - 1).toInt();
            }
        } else
            b = nth.toInt();
    }
    return true;
}

// a helper function for checking nth-arguments
static bool matchNth(int count, int a, int b)
{
    if (!a)
        return count == b;
    else if (a > 0) {
        if (count < b)
            return false;
        return (count - b) % a == 0;
    } else {
        if (count > b)
            return false;
        return (b - count) % (-a) == 0;
    }
}


#ifdef STYLE_SHARING_STATS
static int fraction = 0;
static int total = 0;
#endif

static const unsigned cStyleSearchThreshold = 10;

Node* CSSStyleSelector::locateCousinList(Element* parent, unsigned depth)
{
    if (parent && parent->isStyledElement()) {
        StyledElement* p = static_cast<StyledElement*>(parent);
        if (!p->inlineStyleDecl() && !p->hasID()) {
            Node* r = p->previousSibling();
            unsigned subcount = 0;
            RenderStyle* st = p->renderStyle();
            while (r) {
                if (r->renderStyle() == st)
                    return r->lastChild();
                if (subcount++ == cStyleSearchThreshold)
                    return 0;
                r = r->previousSibling();
            }
            if (!r && depth < cStyleSearchThreshold)
                r = locateCousinList(static_cast<Element*>(parent->parentNode()), depth + 1);
            while (r) {
                if (r->renderStyle() == st)
                    return r->lastChild();
                if (subcount++ == cStyleSearchThreshold)
                    return 0;
                r = r->previousSibling();
            }
        }
    }
    return 0;
}

bool CSSStyleSelector::canShareStyleWithElement(Node* n)
{
    XMLNames* xmlNames = XMLNames::Instance();
    ASSERT(xmlNames);

    if (n->isStyledElement()) {
        StyledElement* s = static_cast<StyledElement*>(n);
        RenderStyle* style = s->renderStyle();
        if (style && !style->unique() &&
            (s->tagQName() == m_element->tagQName()) && !s->hasID() &&
            (s->hasClass() == m_element->hasClass()) && !s->inlineStyleDecl() &&
            (s->hasMappedAttributes() == m_styledElement->hasMappedAttributes()) &&
            (s->isLink() == m_element->isLink()) && 
            !style->affectedByAttributeSelectors() &&
            (s->hovered() == m_element->hovered()) &&
            (s->active() == m_element->active()) &&
            (s->focused() == m_element->focused()) &&
            (s != s->document()->getCSSTarget() && m_element != m_element->document()->getCSSTarget()) &&
            (s->getAttribute(typeAttr) == m_element->getAttribute(typeAttr)) &&
            (s->getAttribute(xmlNames->langAttr) == m_element->getAttribute(xmlNames->langAttr)) &&
            (s->getAttribute(langAttr) == m_element->getAttribute(langAttr)) &&
            (s->getAttribute(readonlyAttr) == m_element->getAttribute(readonlyAttr)) &&
            (s->getAttribute(cellpaddingAttr) == m_element->getAttribute(cellpaddingAttr))) {
            bool isControl = s->isControl();
            if (isControl != m_element->isControl())
                return false;
            if (isControl && (s->isEnabled() != m_element->isEnabled()) ||
                             (s->isIndeterminate() != m_element->isIndeterminate()) ||
                             (s->isChecked() != m_element->isChecked()))
                return false;
            
            if (style->transitions())
                return false;

            bool classesMatch = true;
            if (s->hasClass()) {
                const AtomicString& class1 = m_element->getAttribute(classAttr);
                const AtomicString& class2 = s->getAttribute(classAttr);
                classesMatch = (class1 == class2);
            }
            
            if (classesMatch) {
                bool mappedAttrsMatch = true;
                if (s->hasMappedAttributes())
                    mappedAttrsMatch = s->mappedAttributes()->mapsEquivalent(m_styledElement->mappedAttributes());
                if (mappedAttrsMatch) {
                    bool linksMatch = true;

                    if (s->isLink()) {
                        // We need to check to see if the visited state matches.
                        if (pseudoState == PseudoUnknown) {
                            const Color& linkColor = m_element->document()->linkColor();
                            const Color& visitedColor = m_element->document()->visitedLinkColor();
                            pseudoState = m_checker.checkPseudoState(m_element, style->pseudoState() != PseudoAnyLink || linkColor != visitedColor);
                        }
                        linksMatch = (pseudoState == style->pseudoState());
                    }
                    
                    if (linksMatch)
                        return true;
                }
            }
        }
    }
    return false;
}

RenderStyle* CSSStyleSelector::locateSharedStyle()
{
    if (m_styledElement && !m_styledElement->inlineStyleDecl() && !m_styledElement->hasID() && !m_styledElement->document()->usesSiblingRules()) {
        // Check previous siblings.
        unsigned count = 0;
        Node* n;
        for (n = m_element->previousSibling(); n && !n->isElementNode(); n = n->previousSibling()) { }
        while (n) {
            if (canShareStyleWithElement(n))
                return n->renderStyle();
            if (count++ == cStyleSearchThreshold)
                return 0;
            for (n = n->previousSibling(); n && !n->isElementNode(); n = n->previousSibling()) { }
        }
        if (!n) 
            n = locateCousinList(static_cast<Element*>(m_element->parentNode()));
        while (n) {
            if (canShareStyleWithElement(n))
                return n->renderStyle();
            if (count++ == cStyleSearchThreshold)
                return 0;
            for (n = n->previousSibling(); n && !n->isElementNode(); n = n->previousSibling()) { }
        }        
    }
    return 0;
}

void CSSStyleSelector::matchUARules(int& firstUARule, int& lastUARule)
{
    // First we match rules from the user agent sheet.
    CSSRuleSet* userAgentStyleSheet = m_medium->mediaTypeMatchSpecific("print")
        ? defaultPrintStyle : defaultStyle;
    matchRules(userAgentStyleSheet, firstUARule, lastUARule);

    // In quirks mode, we match rules from the quirks user agent sheet.
    if (!m_checker.m_strictParsing)
        matchRules(defaultQuirksStyle, firstUARule, lastUARule);
        
    // If we're in view source mode, then we match rules from the view source style sheet.
    if (m_checker.m_document->frame() && m_checker.m_document->frame()->inViewSourceMode())
        matchRules(defaultViewSourceStyle, firstUARule, lastUARule);
}

// If resolveForRootDefault is true, style based on user agent style sheet only. This is used in media queries, where
// relative units are interpreted according to document root element style, styled only with UA stylesheet

PassRefPtr<RenderStyle> CSSStyleSelector::styleForElement(Element* e, RenderStyle* defaultParent, bool allowSharing, bool resolveForRootDefault) 
{
    // Once an element has a renderer, we don't try to destroy it, since otherwise the renderer
    // will vanish if a style recalc happens during loading.
    if (allowSharing && !e->document()->haveStylesheetsLoaded() && !e->renderer()) {
        if (!s_styleNotYetAvailable) {
            s_styleNotYetAvailable = new RenderStyle;
            s_styleNotYetAvailable->ref();
            s_styleNotYetAvailable->setDisplay(NONE);
            s_styleNotYetAvailable->font().update(m_fontSelector);
        }
        s_styleNotYetAvailable->ref();
        e->document()->setHasNodesWithPlaceholderStyle();
        return s_styleNotYetAvailable;
    }

    initElementAndPseudoState(e);
    if (allowSharing) 
    {
        RenderStyle* sharedStyle = locateSharedStyle(); 
        if (sharedStyle) 
            return sharedStyle; 
    }
    initForStyleResolve(e, defaultParent);
    m_style = RenderStyle::create();

    if (m_parentStyle)
        m_style->inheritFrom(m_parentStyle);
    else
        m_parentStyle = style();

#if ENABLE(SVG)
    static bool loadedSVGUserAgentSheet;
    if (e->isSVGElement() && !loadedSVGUserAgentSheet) {
        // SVG rules.
        loadedSVGUserAgentSheet = true;
        CSSStyleSheet* svgSheet = parseUASheet(svgUserAgentStyleSheet, sizeof(svgUserAgentStyleSheet));
        defaultStyle->addRulesFromSheet(svgSheet, screenEval());
        defaultPrintStyle->addRulesFromSheet(svgSheet, printEval());
    }
#endif

    int firstUARule = -1, lastUARule = -1;
    int firstUserRule = -1, lastUserRule = -1;
    int firstAuthorRule = -1, lastAuthorRule = -1;
    matchUARules(firstUARule, lastUARule);

    if (!resolveForRootDefault) {
        // 4. Now we check user sheet rules.
        if (m_matchAuthorAndUserStyles)
            matchRules(m_userStyle, firstUserRule, lastUserRule);

        // 5. Now check author rules, beginning first with presentational attributes
        // mapped from HTML.
        if (m_styledElement) {
            // Ask if the HTML element has mapped attributes.
            if (m_styledElement->hasMappedAttributes()) {
                // Walk our attribute list and add in each decl.
                const NamedMappedAttrMap* map = m_styledElement->mappedAttributes();
                for (unsigned i = 0; i < map->length(); i++) {
                    MappedAttribute* attr = map->attributeItem(i);
                    if (attr->decl()) {
                        lastAuthorRule = m_matchedDecls.size();
                        if (firstAuthorRule == -1)
                            firstAuthorRule = lastAuthorRule;
                        addMatchedDeclaration(attr->decl());
                    }
                }
            }

            // Now we check additional mapped declarations.
            // Tables and table cells share an additional mapped rule that must be applied
            // after all attributes, since their mapped style depends on the values of multiple attributes.
            if (m_styledElement->canHaveAdditionalAttributeStyleDecls()) {
                m_additionalAttributeStyleDecls.clear();
                m_styledElement->additionalAttributeStyleDecls(m_additionalAttributeStyleDecls);
                if (!m_additionalAttributeStyleDecls.isEmpty()) {
                    unsigned additionalDeclsSize = m_additionalAttributeStyleDecls.size();
                    if (firstAuthorRule == -1)
                        firstAuthorRule = m_matchedDecls.size();
                    lastAuthorRule = m_matchedDecls.size() + additionalDeclsSize - 1;
                    for (unsigned i = 0; i < additionalDeclsSize; i++)
                        addMatchedDeclaration(m_additionalAttributeStyleDecls[i]);
                }
            }
        }
    
        // 6. Check the rules in author sheets next.
        if (m_matchAuthorAndUserStyles)
            matchRules(m_authorStyle, firstAuthorRule, lastAuthorRule);

        // 7. Now check our inline style attribute.
        if (m_matchAuthorAndUserStyles && m_styledElement) {
            CSSMutableStyleDeclaration* inlineDecl = m_styledElement->inlineStyleDecl();
            if (inlineDecl) {
                lastAuthorRule = m_matchedDecls.size();
                if (firstAuthorRule == -1)
                    firstAuthorRule = lastAuthorRule;
                addMatchedDeclaration(inlineDecl);
            }
        }
    }

    // Now we have all of the matched rules in the appropriate order.  Walk the rules and apply
    // high-priority properties first, i.e., those properties that other properties depend on.
    // The order is (1) high-priority not important, (2) high-priority important, (3) normal not important
    // and (4) normal important.
    m_lineHeightValue = 0;
    applyDeclarations(true, false, 0, m_matchedDecls.size() - 1);
    if (!resolveForRootDefault) {
        applyDeclarations(true, true, firstAuthorRule, lastAuthorRule);
        applyDeclarations(true, true, firstUserRule, lastUserRule);
    }
    applyDeclarations(true, true, firstUARule, lastUARule);
    
    // If our font got dirtied, go ahead and update it now.
    if (m_fontDirty)
        updateFont();

    // Line-height is set when we are sure we decided on the font-size
    if (m_lineHeightValue)
        applyProperty(CSSPropertyLineHeight, m_lineHeightValue);

    // Now do the normal priority UA properties.
    applyDeclarations(false, false, firstUARule, lastUARule);
    
    // Cache our border and background so that we can examine them later.
    cacheBorderAndBackground();
    
    // Now do the author and user normal priority properties and all the !important properties.
    if (!resolveForRootDefault) {
        applyDeclarations(false, false, lastUARule + 1, m_matchedDecls.size() - 1);
        applyDeclarations(false, true, firstAuthorRule, lastAuthorRule);
        applyDeclarations(false, true, firstUserRule, lastUserRule);
    }
    applyDeclarations(false, true, firstUARule, lastUARule);
    
    // If our font got dirtied by one of the non-essential font props, 
    // go ahead and update it a second time.
    if (m_fontDirty)
        updateFont();
    
    // Clean up our style object's display and text decorations (among other fixups).
    adjustRenderStyle(style(), e);

    // If we are a link, cache the determined pseudo-state.
    if (e->isLink())
        m_style->setPseudoState(pseudoState);

    // If we have first-letter pseudo style, do not share this style
    if (m_style->hasPseudoStyle(RenderStyle::FIRST_LETTER))
        m_style->setUnique();

    // Now return the style.
    return m_style.release();
}

PassRefPtr<RenderStyle> CSSStyleSelector::pseudoStyleForElement(RenderStyle::PseudoId pseudo, Element* e, RenderStyle* parentStyle)
{
    if (!e)
        return 0;

    initElementAndPseudoState(e);
    initForStyleResolve(e, parentStyle, pseudo);
    m_style = parentStyle;
    
    // Since we don't use pseudo-elements in any of our quirk/print user agent rules, don't waste time walking
    // those rules.
    
    // Check UA, user and author rules.
    int firstUARule = -1, lastUARule = -1, firstUserRule = -1, lastUserRule = -1, firstAuthorRule = -1, lastAuthorRule = -1;
    matchUARules(firstUARule, lastUARule);

    if (m_matchAuthorAndUserStyles) {
        matchRules(m_userStyle, firstUserRule, lastUserRule);
        matchRules(m_authorStyle, firstAuthorRule, lastAuthorRule);
    }

    if (m_matchedDecls.isEmpty())
        return 0;
    
    m_style = RenderStyle::create();
    if (parentStyle)
        m_style->inheritFrom(parentStyle);

    m_style->noninherited_flags._styleType = pseudo;
   
    m_lineHeightValue = 0;
    // High-priority properties.
    applyDeclarations(true, false, 0, m_matchedDecls.size() - 1);
    applyDeclarations(true, true, firstAuthorRule, lastAuthorRule);
    applyDeclarations(true, true, firstUserRule, lastUserRule);
    applyDeclarations(true, true, firstUARule, lastUARule);
   
    // If our font got dirtied, go ahead and update it now.
    if (m_fontDirty)
        updateFont();

    // Line-height is set when we are sure we decided on the font-size
    if (m_lineHeightValue)
        applyProperty(CSSPropertyLineHeight, m_lineHeightValue);
   
    // Now do the normal priority properties.
    applyDeclarations(false, false, firstUARule, lastUARule);
   
    // Cache our border and background so that we can examine them later.
    cacheBorderAndBackground();
   
    applyDeclarations(false, false, lastUARule + 1, m_matchedDecls.size() - 1);
    applyDeclarations(false, true, firstAuthorRule, lastAuthorRule);
    applyDeclarations(false, true, firstUserRule, lastUserRule);
    applyDeclarations(false, true, firstUARule, lastUARule);
    
    // If our font got dirtied by one of the non-essential font props, 
    // go ahead and update it a second time.
    if (m_fontDirty)
        updateFont();
    // Clean up our style object's display and text decorations (among other fixups).
    adjustRenderStyle(style(), 0);

    // Now return the style.
    return m_style.release();
}

static void addIntrinsicMargins(RenderStyle* style)
{
    // Intrinsic margin value.
    const int intrinsicMargin = 2;
    
    // FIXME: Using width/height alone and not also dealing with min-width/max-width is flawed.
    // FIXME: Using "quirk" to decide the margin wasn't set is kind of lame.
    if (style->width().isIntrinsicOrAuto()) {
        if (style->marginLeft().quirk())
            style->setMarginLeft(Length(intrinsicMargin, Fixed));
        if (style->marginRight().quirk())
            style->setMarginRight(Length(intrinsicMargin, Fixed));
    }

    if (style->height().isAuto()) {
        if (style->marginTop().quirk())
            style->setMarginTop(Length(intrinsicMargin, Fixed));
        if (style->marginBottom().quirk())
            style->setMarginBottom(Length(intrinsicMargin, Fixed));
    }
}

void CSSStyleSelector::adjustRenderStyle(RenderStyle* style, Element *e)
{
    // Cache our original display.
    style->setOriginalDisplay(style->display());

    if (style->display() != NONE) {
        // If we have a <td> that specifies a float property, in quirks mode we just drop the float
        // property.
        // Sites also commonly use display:inline/block on <td>s and <table>s.  In quirks mode we force
        // these tags to retain their display types.
        if (!m_checker.m_strictParsing && e) {
            if (e->hasTagName(tdTag)) {
                style->setDisplay(TABLE_CELL);
                style->setFloating(FNONE);
            }
            else if (e->hasTagName(tableTag))
                style->setDisplay(style->isDisplayInlineType() ? INLINE_TABLE : TABLE);
        }

        // Tables never support the -webkit-* values for text-align and will reset back to the default.
        if (e && e->hasTagName(tableTag) && (style->textAlign() == WEBKIT_LEFT || style->textAlign() == WEBKIT_CENTER || style->textAlign() == WEBKIT_RIGHT))
            style->setTextAlign(TAAUTO);

        // Frames and framesets never honor position:relative or position:absolute.  This is necessary to
        // fix a crash where a site tries to position these objects.  They also never honor display.
        if (e && (e->hasTagName(frameTag) || e->hasTagName(framesetTag))) {
            style->setPosition(StaticPosition);
            style->setDisplay(BLOCK);
        }

        // Table headers with a text-align of auto will change the text-align to center.
        if (e && e->hasTagName(thTag) && style->textAlign() == TAAUTO)
            style->setTextAlign(CENTER);
        
        // Mutate the display to BLOCK or TABLE for certain cases, e.g., if someone attempts to
        // position or float an inline, compact, or run-in.  Cache the original display, since it
        // may be needed for positioned elements that have to compute their static normal flow
        // positions.  We also force inline-level roots to be block-level.
        if (style->display() != BLOCK && style->display() != TABLE && style->display() != BOX &&
            (style->position() == AbsolutePosition || style->position() == FixedPosition || style->floating() != FNONE ||
             (e && e->document()->documentElement() == e))) {
            if (style->display() == INLINE_TABLE)
                style->setDisplay(TABLE);
            else if (style->display() == INLINE_BOX)
                style->setDisplay(BOX);
            else if (style->display() == LIST_ITEM) {
                // It is a WinIE bug that floated list items lose their bullets, so we'll emulate the quirk,
                // but only in quirks mode.
                if (!m_checker.m_strictParsing && style->floating() != FNONE)
                    style->setDisplay(BLOCK);
            }
            else
                style->setDisplay(BLOCK);
        }
        
        // After performing the display mutation, check table rows.  We do not honor position:relative on
        // table rows or cells.  This has been established in CSS2.1 (and caused a crash in containingBlock()
        // on some sites).
        if ((style->display() == TABLE_HEADER_GROUP || style->display() == TABLE_ROW_GROUP ||
             style->display() == TABLE_FOOTER_GROUP || style->display() == TABLE_ROW || style->display() == TABLE_CELL) &&
             style->position() == RelativePosition)
            style->setPosition(StaticPosition);
    }

    // Make sure our z-index value is only applied if the object is positioned.
    if (style->position() == StaticPosition)
        style->setHasAutoZIndex();

    // Auto z-index becomes 0 for the root element and transparent objects.  This prevents
    // cases where objects that should be blended as a single unit end up with a non-transparent
    // object wedged in between them.  Auto z-index also becomes 0 for objects that specify transforms/masks/reflections.
    if (style->hasAutoZIndex() && ((e && e->document()->documentElement() == e) || style->opacity() < 1.0f || 
        style->hasTransform() || style->hasMask() || style->boxReflect()))
        style->setZIndex(0);
    
    // Button, legend, input, select and textarea all consider width values of 'auto' to be 'intrinsic'.
    // This will be important when we use block flows for all form controls.
    if (e && (e->hasTagName(legendTag) || e->hasTagName(buttonTag) || e->hasTagName(inputTag) ||
              e->hasTagName(selectTag) || e->hasTagName(textareaTag))) {
        if (style->width().isAuto())
            style->setWidth(Length(Intrinsic));
    }

    // Finally update our text decorations in effect, but don't allow text-decoration to percolate through
    // tables, inline blocks, inline tables, or run-ins.
    if (style->display() == TABLE || style->display() == INLINE_TABLE || style->display() == RUN_IN
        || style->display() == INLINE_BLOCK || style->display() == INLINE_BOX)
        style->setTextDecorationsInEffect(style->textDecoration());
    else
        style->addToTextDecorationsInEffect(style->textDecoration());
    
    // If either overflow value is not visible, change to auto.
    if (style->overflowX() == OMARQUEE && style->overflowY() != OMARQUEE)
        style->setOverflowY(OMARQUEE);
    else if (style->overflowY() == OMARQUEE && style->overflowX() != OMARQUEE)
        style->setOverflowX(OMARQUEE);
    else if (style->overflowX() == OVISIBLE && style->overflowY() != OVISIBLE)
        style->setOverflowX(OAUTO);
    else if (style->overflowY() == OVISIBLE && style->overflowX() != OVISIBLE)
        style->setOverflowY(OAUTO);

    // Table rows, sections and the table itself will support overflow:hidden and will ignore scroll/auto.
    // FIXME: Eventually table sections will support auto and scroll.
    if (style->display() == TABLE || style->display() == INLINE_TABLE ||
        style->display() == TABLE_ROW_GROUP || style->display() == TABLE_ROW) {
        if (style->overflowX() != OVISIBLE && style->overflowX() != OHIDDEN) 
            style->setOverflowX(OVISIBLE);
        if (style->overflowY() != OVISIBLE && style->overflowY() != OHIDDEN) 
            style->setOverflowY(OVISIBLE);
    }

    // Cull out any useless layers and also repeat patterns into additional layers.
    style->adjustBackgroundLayers();
    style->adjustMaskLayers();

    // Do the same for transitions.
    style->adjustTransitions();

    // Important: Intrinsic margins get added to controls before the theme has adjusted the style, since the theme will
    // alter fonts and heights/widths.
    if (e && e->isControl() && style->fontSize() >= 11) {
        // Don't apply intrinsic margins to image buttons.  The designer knows how big the images are,
        // so we have to treat all image buttons as though they were explicitly sized.
        if (!e->hasTagName(inputTag) || static_cast<HTMLInputElement*>(e)->inputType() != HTMLInputElement::IMAGE)
            addIntrinsicMargins(style);
    }

    // Let the theme also have a crack at adjusting the style.
    if (style->hasAppearance())
        theme()->adjustStyle(this, style, e, m_hasUAAppearance, m_borderData, m_backgroundData, m_backgroundColor);

#if ENABLE(SVG)
    if (e && e->isSVGElement()) {
        // Spec: http://www.w3.org/TR/SVG/masking.html#OverflowProperty
        if (style->overflowY() == OSCROLL)
            style->setOverflowY(OHIDDEN);
        else if (style->overflowY() == OAUTO)
            style->setOverflowY(OVISIBLE);

        if (style->overflowX() == OSCROLL)
            style->setOverflowX(OHIDDEN);
        else if (style->overflowX() == OAUTO)
            style->setOverflowX(OVISIBLE);

        // Only the root <svg> element in an SVG document fragment tree honors css position
        if (!(e->hasTagName(SVGNames::svgTag) && e->parentNode() && !e->parentNode()->isSVGElement()))
            style->setPosition(RenderStyle::initialPosition());
    }
#endif
}

void CSSStyleSelector::updateFont()
{
    checkForTextSizeAdjust();
    checkForGenericFamilyChange(style(), m_parentStyle);
    checkForZoomChange(style(), m_parentStyle);
    m_style->font().update(m_fontSelector);
    m_fontDirty = false;
}

void CSSStyleSelector::cacheBorderAndBackground()
{
    m_hasUAAppearance = m_style->hasAppearance();
    if (m_hasUAAppearance) {
        m_borderData = m_style->border();
        m_backgroundData = *m_style->backgroundLayers();
        m_backgroundColor = m_style->backgroundColor();
    }
}

PassRefPtr<CSSRuleList> CSSStyleSelector::styleRulesForElement(Element* e, bool authorOnly)
{
    if (!e || !e->document()->haveStylesheetsLoaded())
        return 0;

    m_checker.m_collectRulesOnly = true;

    initElementAndPseudoState(e);
    initForStyleResolve(e);

    if (!authorOnly) {
        int firstUARule = -1, lastUARule = -1;
        // First we match rules from the user agent sheet.
        matchUARules(firstUARule, lastUARule);

        // Now we check user sheet rules.
        if (m_matchAuthorAndUserStyles) {
            int firstUserRule = -1, lastUserRule = -1;
            matchRules(m_userStyle, firstUserRule, lastUserRule);
        }
    }

    if (m_matchAuthorAndUserStyles) {
        // Check the rules in author sheets.
        int firstAuthorRule = -1, lastAuthorRule = -1;
        matchRules(m_authorStyle, firstAuthorRule, lastAuthorRule);
    }

    m_checker.m_collectRulesOnly = false;
   
    return m_ruleList.release();
}

PassRefPtr<CSSRuleList> CSSStyleSelector::pseudoStyleRulesForElement(Element*, const String& pseudoStyle, bool authorOnly)
{
    // FIXME: Implement this.
    return 0;
}

bool CSSStyleSelector::checkSelector(CSSSelector* sel)
{
    m_dynamicPseudo = RenderStyle::NOPSEUDO;

    // Check the selector
    SelectorMatch match = m_checker.checkSelector(sel, m_element, &m_selectorAttrs, m_dynamicPseudo, true, false, style(), m_parentStyle);
    if (match != SelectorMatches)
        return false;

    if (m_checker.m_pseudoStyle != RenderStyle::NOPSEUDO && m_checker.m_pseudoStyle != m_dynamicPseudo)
        return false;

    return true;
}

// Recursive check of selectors and combinators
// It can return 3 different values:
// * SelectorMatches         - the selector matches the element e
// * SelectorFailsLocally    - the selector fails for the element e
// * SelectorFailsCompletely - the selector fails for e and any sibling or ancestor of e
CSSStyleSelector::SelectorMatch CSSStyleSelector::SelectorChecker::checkSelector(CSSSelector* sel, Element* e, HashSet<AtomicStringImpl*>* selectorAttrs, RenderStyle::PseudoId& dynamicPseudo, bool isAncestor, bool isSubSelector, RenderStyle* elementStyle, RenderStyle* elementParentStyle) const
{
#if ENABLE(SVG)
    // Spec: CSS2 selectors cannot be applied to the (conceptually) cloned DOM tree
    // because its contents are not part of the formal document structure.
    if (e->isSVGElement() && e->isShadowNode())
        return SelectorFailsCompletely;
#endif

    // first selector has to match
    if (!checkOneSelector(sel, e, selectorAttrs, dynamicPseudo, isAncestor, isSubSelector, elementStyle, elementParentStyle))
        return SelectorFailsLocally;

    // The rest of the selectors has to match
    CSSSelector::Relation relation = sel->relation();

    // Prepare next sel
    sel = sel->m_tagHistory;
    if (!sel)
        return SelectorMatches;

    if (relation != CSSSelector::SubSelector)
        // Bail-out if this selector is irrelevant for the pseudoStyle
        if (m_pseudoStyle != RenderStyle::NOPSEUDO && m_pseudoStyle != dynamicPseudo)
            return SelectorFailsCompletely;

    switch (relation) {
        case CSSSelector::Descendant:
            while (true) {
                Node* n = e->parentNode();
                if (!n || !n->isElementNode())
                    return SelectorFailsCompletely;
                e = static_cast<Element*>(n);
                SelectorMatch match = checkSelector(sel, e, selectorAttrs, dynamicPseudo, true, false);
                if (match != SelectorFailsLocally)
                    return match;
            }
            break;
        case CSSSelector::Child:
        {
            Node* n = e->parentNode();
            if (!n || !n->isElementNode())
                return SelectorFailsCompletely;
            e = static_cast<Element*>(n);
            return checkSelector(sel, e, selectorAttrs, dynamicPseudo, true, false);
        }
        case CSSSelector::DirectAdjacent:
        {
            if (!m_collectRulesOnly && e->parentNode() && e->parentNode()->isElementNode()) {
                RenderStyle* parentStyle = elementStyle ? elementParentStyle : e->parentNode()->renderStyle();
                if (parentStyle)
                    parentStyle->setChildrenAffectedByDirectAdjacentRules();
            }
            Node* n = e->previousSibling();
            while (n && !n->isElementNode())
                n = n->previousSibling();
            if (!n)
                return SelectorFailsLocally;
            e = static_cast<Element*>(n);
            return checkSelector(sel, e, selectorAttrs, dynamicPseudo, false, false); 
        }
        case CSSSelector::IndirectAdjacent:
            if (!m_collectRulesOnly && e->parentNode() && e->parentNode()->isElementNode()) {
                RenderStyle* parentStyle = elementStyle ? elementParentStyle : e->parentNode()->renderStyle();
                if (parentStyle)
                    parentStyle->setChildrenAffectedByForwardPositionalRules();
            }
            while (true) {
                Node* n = e->previousSibling();
                while (n && !n->isElementNode())
                    n = n->previousSibling();
                if (!n)
                    return SelectorFailsLocally;
                e = static_cast<Element*>(n);
                SelectorMatch match = checkSelector(sel, e, selectorAttrs, dynamicPseudo, false, false);
                if (match != SelectorFailsLocally)
                    return match;
            };
            break;
        case CSSSelector::SubSelector:
            // a selector is invalid if something follows a pseudo-element
            if (elementStyle && dynamicPseudo != RenderStyle::NOPSEUDO)
                return SelectorFailsCompletely;
            return checkSelector(sel, e, selectorAttrs, dynamicPseudo, isAncestor, true, elementStyle, elementParentStyle);
    }

    return SelectorFailsCompletely;
}

static void addLocalNameToSet(HashSet<AtomicStringImpl*>* set, const QualifiedName& qName)
{
    set->add(qName.localName().impl());
}

static HashSet<AtomicStringImpl*>* createHtmlCaseInsensitiveAttributesSet()
{
    // This is the list of attributes in HTML 4.01 with values marked as "[CI]" or case-insensitive
    // Mozilla treats all other values as case-sensitive, thus so do we.
    HashSet<AtomicStringImpl*>* attrSet = new HashSet<AtomicStringImpl*>;

    addLocalNameToSet(attrSet, accept_charsetAttr);
    addLocalNameToSet(attrSet, acceptAttr);
    addLocalNameToSet(attrSet, alignAttr);
    addLocalNameToSet(attrSet, alinkAttr);
    addLocalNameToSet(attrSet, axisAttr);
    addLocalNameToSet(attrSet, bgcolorAttr);
    addLocalNameToSet(attrSet, charsetAttr);
    addLocalNameToSet(attrSet, checkedAttr);
    addLocalNameToSet(attrSet, clearAttr);
    addLocalNameToSet(attrSet, codetypeAttr);
    addLocalNameToSet(attrSet, colorAttr);
    addLocalNameToSet(attrSet, compactAttr);
    addLocalNameToSet(attrSet, declareAttr);
    addLocalNameToSet(attrSet, deferAttr);
    addLocalNameToSet(attrSet, dirAttr);
    addLocalNameToSet(attrSet, disabledAttr);
    addLocalNameToSet(attrSet, enctypeAttr);
    addLocalNameToSet(attrSet, faceAttr);
    addLocalNameToSet(attrSet, frameAttr);
    addLocalNameToSet(attrSet, hreflangAttr);
    addLocalNameToSet(attrSet, http_equivAttr);
    addLocalNameToSet(attrSet, langAttr);
    addLocalNameToSet(attrSet, languageAttr);
    addLocalNameToSet(attrSet, linkAttr);
    addLocalNameToSet(attrSet, mediaAttr);
    addLocalNameToSet(attrSet, methodAttr);
    addLocalNameToSet(attrSet, multipleAttr);
    addLocalNameToSet(attrSet, nohrefAttr);
    addLocalNameToSet(attrSet, noresizeAttr);
    addLocalNameToSet(attrSet, noshadeAttr);
    addLocalNameToSet(attrSet, nowrapAttr);
    addLocalNameToSet(attrSet, readonlyAttr);
    addLocalNameToSet(attrSet, relAttr);
    addLocalNameToSet(attrSet, revAttr);
    addLocalNameToSet(attrSet, rulesAttr);
    addLocalNameToSet(attrSet, scopeAttr);
    addLocalNameToSet(attrSet, scrollingAttr);
    addLocalNameToSet(attrSet, selectedAttr);
    addLocalNameToSet(attrSet, shapeAttr);
    addLocalNameToSet(attrSet, targetAttr);
    addLocalNameToSet(attrSet, textAttr);
    addLocalNameToSet(attrSet, typeAttr);
    addLocalNameToSet(attrSet, valignAttr);
    addLocalNameToSet(attrSet, valuetypeAttr);
    addLocalNameToSet(attrSet, vlinkAttr);

    return attrSet;
}

static bool htmlAttributeHasCaseInsensitiveValue(const QualifiedName& attr)
{
    // 3/27/09 CSidhall - Move out for mem leak
    // static HashSet<AtomicStringImpl*>* htmlCaseInsensitiveAttributesSet = createHtmlCaseInsensitiveAttributesSet();
    if(!sHtmlCaseInsensitiveAttributesSet)
        sHtmlCaseInsensitiveAttributesSet = createHtmlCaseInsensitiveAttributesSet();    

    bool isPossibleHTMLAttr = !attr.hasPrefix() && (attr.namespaceURI() == nullAtom);
    return isPossibleHTMLAttr && sHtmlCaseInsensitiveAttributesSet->contains(attr.localName().impl());
}

bool CSSStyleSelector::SelectorChecker::checkOneSelector(CSSSelector* sel, Element* e, HashSet<AtomicStringImpl*>* selectorAttrs, RenderStyle::PseudoId& dynamicPseudo, bool isAncestor, bool isSubSelector, RenderStyle* elementStyle, RenderStyle* elementParentStyle) const
{
    if (!e)
        return false;

    if (sel->hasTag()) {
        const AtomicString& selLocalName = sel->m_tag.localName();
        if (selLocalName != starAtom && selLocalName != e->localName())
            return false;
        const AtomicString& selNS = sel->m_tag.namespaceURI();
        if (selNS != starAtom && selNS != e->namespaceURI())
            return false;
    }

    if (sel->hasAttribute()) {
        if (sel->m_match == CSSSelector::Class)
            return e->hasClass() && static_cast<StyledElement*>(e)->classNames().contains(sel->m_value);

        if (sel->m_match == CSSSelector::Id)
            return e->hasID() && e->getIDAttribute() == sel->m_value;

        // FIXME: Handle the case were elementStyle is 0.
        if (elementStyle && (!e->isStyledElement() || (!static_cast<StyledElement*>(e)->isMappedAttribute(sel->m_attr) && sel->m_attr != typeAttr && sel->m_attr != readonlyAttr))) {
            elementStyle->setAffectedByAttributeSelectors(); // Special-case the "type" and "readonly" attributes so input form controls can share style.
            if (selectorAttrs)
                selectorAttrs->add(sel->m_attr.localName().impl());
        }

        const AtomicString& value = e->getAttribute(sel->m_attr);
        if (value.isNull())
            return false; // attribute is not set

        bool caseSensitive = !m_documentIsHTML || !htmlAttributeHasCaseInsensitiveValue(sel->m_attr);

        switch (sel->m_match) {
        case CSSSelector::Exact:
            if (caseSensitive ? sel->m_value != value : !equalIgnoringCase(sel->m_value, value))
                return false;
            break;
        case CSSSelector::List:
        {
            // Ignore empty selectors or selectors containing spaces
            if (sel->m_value.contains(' ') || sel->m_value.isEmpty())
                return false;

            int startSearchAt = 0;
            while (true) {
                int foundPos = value.find(sel->m_value, startSearchAt, caseSensitive);
                if (foundPos == -1)
                    return false;
                if (foundPos == 0 || value[foundPos-1] == ' ') {
                    unsigned endStr = foundPos + sel->m_value.length();
                    if (endStr == value.length() || value[endStr] == ' ')
                        break; // We found a match.
                }
               
                // No match.  Keep looking.
                startSearchAt = foundPos + 1;
            }
            break;
        }
        case CSSSelector::Contain:
            if (!value.contains(sel->m_value, caseSensitive) || sel->m_value.isEmpty())
                return false;
            break;
        case CSSSelector::Begin:
            if (!value.startsWith(sel->m_value, caseSensitive) || sel->m_value.isEmpty())
                return false;
            break;
        case CSSSelector::End:
            if (!value.endsWith(sel->m_value, caseSensitive) || sel->m_value.isEmpty())
                return false;
            break;
        case CSSSelector::Hyphen:
            if (value.length() < sel->m_value.length())
                return false;
            if (!value.startsWith(sel->m_value, caseSensitive))
                return false;
            // It they start the same, check for exact match or following '-':
            if (value.length() != sel->m_value.length() && value[sel->m_value.length()] != '-')
                return false;
            break;
        case CSSSelector::PseudoClass:
        case CSSSelector::PseudoElement:
        default:
            break;
        }
    }
    if (sel->m_match == CSSSelector::PseudoClass) {
        switch (sel->pseudoType()) {
            // Pseudo classes:
            case CSSSelector::PseudoEmpty: {
                bool result = true;
                for (Node* n = e->firstChild(); n; n = n->nextSibling()) {
                    if (n->isElementNode()) {
                        result = false;
                        break;
                    } else if (n->isTextNode()) {
                        Text* textNode = static_cast<Text*>(n);
                        if (!textNode->data().isEmpty()) {
                            result = false;
                            break;
                        }
                    }
                }
                if (!m_collectRulesOnly) {
                    if (elementStyle)
                        elementStyle->setEmptyState(result);
                    else if (e->renderStyle() && (e->document()->usesSiblingRules() || e->renderStyle()->unique()))
                        e->renderStyle()->setEmptyState(result);
                }
                return result;
            }
            case CSSSelector::PseudoFirstChild: {
                // first-child matches the first child that is an element
                if (e->parentNode() && e->parentNode()->isElementNode()) {
                    bool result = false;
                    Node* n = e->previousSibling();
                    while (n && !n->isElementNode())
                        n = n->previousSibling();
                    if (!n)
                        result = true;
                    if (!m_collectRulesOnly) {
                        RenderStyle* childStyle = elementStyle ? elementStyle : e->renderStyle();
                        RenderStyle* parentStyle = elementStyle ? elementParentStyle : e->parentNode()->renderStyle();
                        if (parentStyle)
                            parentStyle->setChildrenAffectedByFirstChildRules();
                        if (result && childStyle)
                            childStyle->setFirstChildState();
                    }
                    return result;
                }
                break;
            }
            case CSSSelector::PseudoFirstOfType: {
                // first-of-type matches the first element of its type
                if (e->parentNode() && e->parentNode()->isElementNode()) {
                    bool result = false;
                    const QualifiedName& type = e->tagQName();
                    Node* n = e->previousSibling();
                    while (n) {
                        if (n->isElementNode() && static_cast<Element*>(n)->hasTagName(type))
                            break;
                        n = n->previousSibling();
                    }
                    if (!n)
                        result = true;
                    if (!m_collectRulesOnly) {
                        RenderStyle* parentStyle = elementStyle ? elementParentStyle : e->parentNode()->renderStyle();
                        if (parentStyle)
                            parentStyle->setChildrenAffectedByForwardPositionalRules();
                    }
                    return result;
                }
                break;
            }
            case CSSSelector::PseudoLastChild: {
                // last-child matches the last child that is an element
                if (e->parentNode() && e->parentNode()->isElementNode()) {
                    Element* parentNode = static_cast<Element*>(e->parentNode());
                    bool result = false;
                    if (parentNode->isFinishedParsingChildren()) {
                        Node* n = e->nextSibling();
                        while (n && !n->isElementNode())
                            n = n->nextSibling();
                        if (!n)
                            result = true;
                    }
                    if (!m_collectRulesOnly) {
                        RenderStyle* childStyle = elementStyle ? elementStyle : e->renderStyle();
                        RenderStyle* parentStyle = elementStyle ? elementParentStyle : parentNode->renderStyle();
                        if (parentStyle)
                            parentStyle->setChildrenAffectedByLastChildRules();
                        if (result && childStyle)
                            childStyle->setLastChildState();
                    }
                    return result;
                }
                break;
            }
            case CSSSelector::PseudoLastOfType: {
                // last-of-type matches the last element of its type
                if (e->parentNode() && e->parentNode()->isElementNode()) {
                    Element* parentNode = static_cast<Element*>(e->parentNode());
                    if (!m_collectRulesOnly) {
                        RenderStyle* parentStyle = elementStyle ? elementParentStyle : parentNode->renderStyle();
                        if (parentStyle)
                            parentStyle->setChildrenAffectedByBackwardPositionalRules();
                    }
                    if (!parentNode->isFinishedParsingChildren())
                        return false;
                    bool result = false;
                    const QualifiedName& type = e->tagQName();
                    Node* n = e->nextSibling();
                    while (n) {
                        if (n->isElementNode() && static_cast<Element*>(n)->hasTagName(type))
                            break;
                        n = n->nextSibling();
                    }
                    if (!n)
                        result = true;
                    return result;
                }
                break;
            }
            case CSSSelector::PseudoOnlyChild: {
                if (e->parentNode() && e->parentNode()->isElementNode()) {
                    Element* parentNode = static_cast<Element*>(e->parentNode());
                    bool firstChild = false;
                    bool lastChild = false;
                   
                    Node* n = e->previousSibling();
                    while (n && !n->isElementNode())
                        n = n->previousSibling();
                    if (!n)
                        firstChild = true;
                    if (firstChild && parentNode->isFinishedParsingChildren()) {
                        n = e->nextSibling();
                        while (n && !n->isElementNode())
                            n = n->nextSibling();
                        if (!n)
                            lastChild = true;
                    }
                    if (!m_collectRulesOnly) {
                        RenderStyle* childStyle = elementStyle ? elementStyle : e->renderStyle();
                        RenderStyle* parentStyle = elementStyle ? elementParentStyle : parentNode->renderStyle();
                        if (parentStyle) {
                            parentStyle->setChildrenAffectedByFirstChildRules();
                            parentStyle->setChildrenAffectedByLastChildRules();
                        }
                        if (firstChild && childStyle)
                            childStyle->setFirstChildState();
                        if (lastChild && childStyle)
                            childStyle->setLastChildState();
                    }
                    return firstChild && lastChild;
                }
                break;
            }
            case CSSSelector::PseudoOnlyOfType: {
                // FIXME: This selector is very slow.
                if (e->parentNode() && e->parentNode()->isElementNode()) {
                    Element* parentNode = static_cast<Element*>(e->parentNode());
                    if (!m_collectRulesOnly) {
                        RenderStyle* parentStyle = elementStyle ? elementParentStyle : parentNode->renderStyle();
                        if (parentStyle) {
                            parentStyle->setChildrenAffectedByForwardPositionalRules();
                            parentStyle->setChildrenAffectedByBackwardPositionalRules();
                        }
                    }
                    if (!parentNode->isFinishedParsingChildren())
                        return false;
                    bool firstChild = false;
                    bool lastChild = false;
                    const QualifiedName& type = e->tagQName();
                    Node* n = e->previousSibling();
                    while (n) {
                        if (n->isElementNode() && static_cast<Element*>(n)->hasTagName(type))
                            break;
                        n = n->previousSibling();
                    }
                    if (!n)
                        firstChild = true;
                    if (firstChild) {
                        n = e->nextSibling();
                        while (n) {
                            if (n->isElementNode() && static_cast<Element*>(n)->hasTagName(type))
                                break;
                            n = n->nextSibling();
                        }
                        if (!n)
                            lastChild = true;
                    }
                    return firstChild && lastChild;
                }
                break;
            }
            case CSSSelector::PseudoNthChild: {
                int a, b;
                // calculate a and b every time we run through checkOneSelector
                // this should probably be saved after we calculate it once, but currently
                // would require increasing the size of CSSSelector
                if (!parseNth(sel->m_argument, a, b))
                    break;
                if (e->parentNode() && e->parentNode()->isElementNode()) {
                    int count = 1;
                    Node* n = e->previousSibling();
                    while (n) {
                        if (n->isElementNode()) {
                            RenderStyle* s = n->renderStyle();
                            unsigned index = s ? s->childIndex() : 0;
                            if (index) {
                                count += index;
                                break;
                            }
                            count++;
                        }
                        n = n->previousSibling();
                    }
                    
                    if (!m_collectRulesOnly) {
                        RenderStyle* childStyle = elementStyle ? elementStyle : e->renderStyle();
                        RenderStyle* parentStyle = elementStyle ? elementParentStyle : e->parentNode()->renderStyle();
                        if (childStyle)
                            childStyle->setChildIndex(count);
                        if (parentStyle)
                            parentStyle->setChildrenAffectedByForwardPositionalRules();
                    }
                    
                    if (matchNth(count, a, b))
                        return true;
                }
                break;
            }
            case CSSSelector::PseudoNthOfType: {
                // FIXME: This selector is very slow.
                int a, b;
                // calculate a and b every time we run through checkOneSelector (see above)
                if (!parseNth(sel->m_argument, a, b))
                    break;
                if (e->parentNode() && e->parentNode()->isElementNode()) {
                    int count = 1;
                    const QualifiedName& type = e->tagQName();
                    Node* n = e->previousSibling();
                    while (n) {
                        if (n->isElementNode() && static_cast<Element*>(n)->hasTagName(type))
                            count++;
                        n = n->previousSibling();
                    }
                    
                    if (!m_collectRulesOnly) {
                        RenderStyle* parentStyle = elementStyle ? elementParentStyle : e->parentNode()->renderStyle();
                        if (parentStyle)
                            parentStyle->setChildrenAffectedByForwardPositionalRules();
                    }

                    if (matchNth(count, a, b))
                        return true;
                }
                break;
            }
            case CSSSelector::PseudoNthLastChild: {
                int a, b;
                // calculate a and b every time we run through checkOneSelector
                // this should probably be saved after we calculate it once, but currently
                // would require increasing the size of CSSSelector
                if (!parseNth(sel->m_argument, a, b))
                    break;
                if (e->parentNode() && e->parentNode()->isElementNode()) {
                    Element* parentNode = static_cast<Element*>(e->parentNode());
                    if (!m_collectRulesOnly) {
                        RenderStyle* parentStyle = elementStyle ? elementParentStyle : parentNode->renderStyle();
                        if (parentStyle)
                            parentStyle->setChildrenAffectedByBackwardPositionalRules();
                    }
                    if (!parentNode->isFinishedParsingChildren())
                        return false;
                    int count = 1;
                    Node* n = e->nextSibling();
                    while (n) {
                        if (n->isElementNode())
                            count++;
                        n = n->nextSibling();
                    }
                    if (matchNth(count, a, b))
                        return true;
                }
                break;
            }
            case CSSSelector::PseudoNthLastOfType: {
                // FIXME: This selector is very slow.
                int a, b;
                // calculate a and b every time we run through checkOneSelector (see above)
                if (!parseNth(sel->m_argument, a, b))
                    break;
                if (e->parentNode() && e->parentNode()->isElementNode()) {
                    Element* parentNode = static_cast<Element*>(e->parentNode());
                    if (!m_collectRulesOnly) {
                        RenderStyle* parentStyle = elementStyle ? elementParentStyle : parentNode->renderStyle();
                        if (parentStyle)
                            parentStyle->setChildrenAffectedByBackwardPositionalRules();
                    }
                    if (!parentNode->isFinishedParsingChildren())
                        return false;
                    int count = 1;
                    const QualifiedName& type = e->tagQName();
                    Node* n = e->nextSibling();
                    while (n) {
                        if (n->isElementNode() && static_cast<Element*>(n)->hasTagName(type))
                            count++;
                        n = n->nextSibling();
                    }
                    if (matchNth(count, a, b))
                        return true;
                }
                break;
            }
            case CSSSelector::PseudoTarget:
                if (e == e->document()->getCSSTarget())
                    return true;
                break;
            case CSSSelector::PseudoAnyLink:
                if (pseudoState == PseudoUnknown)
                    pseudoState = checkPseudoState(e, false);
                if (pseudoState == PseudoAnyLink || pseudoState == PseudoLink || pseudoState == PseudoVisited)
                    return true;
                break;
            case CSSSelector::PseudoAutofill:
                if (e && e->hasTagName(inputTag))
                    return static_cast<HTMLInputElement*>(e)->autofilled();
                break;
            case CSSSelector::PseudoLink:
                if (pseudoState == PseudoUnknown || pseudoState == PseudoAnyLink)
                    pseudoState = checkPseudoState(e);
                if (pseudoState == PseudoLink)
                    return true;
                break;
            case CSSSelector::PseudoVisited:
                if (pseudoState == PseudoUnknown || pseudoState == PseudoAnyLink)
                    pseudoState = checkPseudoState(e);
                if (pseudoState == PseudoVisited)
                    return true;
                break;
            case CSSSelector::PseudoDrag: {
                if (elementStyle)
                    elementStyle->setAffectedByDragRules(true);
                else if (e->renderStyle())
                    e->renderStyle()->setAffectedByDragRules(true);
                if (e->renderer() && e->renderer()->isDragging())
                    return true;
                break;
            }
            case CSSSelector::PseudoFocus:
                if (e && e->focused() && e->document()->frame()->selection()->isFocusedAndActive())
                    return true;
                break;
            case CSSSelector::PseudoHover: {
                // If we're in quirks mode, then hover should never match anchors with no
                // href and *:hover should not match anything.  This is important for sites like wsj.com.
                if (m_strictParsing || isSubSelector || (sel->hasTag() && !e->hasTagName(aTag)) || e->isLink()) {
                    if (elementStyle)
                        elementStyle->setAffectedByHoverRules(true);
                    else if (e->renderStyle())
                        e->renderStyle()->setAffectedByHoverRules(true);
                    if (e->hovered())
                        return true;
                }
                break;
            }
            case CSSSelector::PseudoActive:
                // If we're in quirks mode, then :active should never match anchors with no
                // href and *:active should not match anything. 
                if (m_strictParsing || isSubSelector || (sel->hasTag() && !e->hasTagName(aTag)) || e->isLink()) {
                    if (elementStyle)
                        elementStyle->setAffectedByActiveRules(true);
                    else if (e->renderStyle())
                        e->renderStyle()->setAffectedByActiveRules(true);
                    if (e->active())
                        return true;
                }
                break;
            case CSSSelector::PseudoEnabled:
                if (e && e->isControl() && !e->isInputTypeHidden())
                    // The UI spec states that you can't match :enabled unless you are an object that can
                    // "receive focus and be activated."  We will limit matching of this pseudo-class to elements
                    // that are non-"hidden" controls.
                    return e->isEnabled();                    
                break;
            case CSSSelector::PseudoDisabled:
                if (e && e->isControl() && !e->isInputTypeHidden())
                    // The UI spec states that you can't match :enabled unless you are an object that can
                    // "receive focus and be activated."  We will limit matching of this pseudo-class to elements
                    // that are non-"hidden" controls.
                    return !e->isEnabled();                    
                break;
            case CSSSelector::PseudoChecked:
                // Even though WinIE allows checked and indeterminate to co-exist, the CSS selector spec says that
                // you can't be both checked and indeterminate.  We will behave like WinIE behind the scenes and just
                // obey the CSS spec here in the test for matching the pseudo.
                if (e && e->isChecked() && !e->isIndeterminate())
                    return true;
                break;
            case CSSSelector::PseudoIndeterminate:
                if (e && e->isIndeterminate())
                    return true;
                break;
            case CSSSelector::PseudoRoot:
                if (e == e->document()->documentElement())
                    return true;
                break;
            case CSSSelector::PseudoLang: {
                Node* n = e;
                AtomicString value;
                // The language property is inherited, so we iterate over the parents
                // to find the first language.
                while (n && value.isEmpty()) 
                {
                    if (n->isElementNode()) 
                    {
                        XMLNames* xmlNames = XMLNames::Instance();
                        ASSERT(xmlNames);
                        // Spec: xml:lang takes precedence -- http://www.w3.org/TR/xhtml1/#C_7
                        value = static_cast<Element*>(n)->getAttribute(xmlNames->langAttr);
                        if (value.isEmpty())
                            value = static_cast<Element*>(n)->getAttribute(langAttr);
                    } 
                    else if (n->isDocumentNode())
                    {
                        // checking the MIME content-language
                        value = static_cast<Document*>(n)->contentLanguage();
                    }

                    n = n->parent();
                }
                if (value.isEmpty() || !value.startsWith(sel->m_argument, false))
                    break;
                if (value.length() != sel->m_argument.length() && value[sel->m_argument.length()] != '-')
                    break;
                return true;
            }
            case CSSSelector::PseudoNot: {
                // check the simple selector
                for (CSSSelector* subSel = sel->m_simpleSelector; subSel; subSel = subSel->m_tagHistory) {
                    // :not cannot nest. I don't really know why this is a
                    // restriction in CSS3, but it is, so let's honour it.
                    if (subSel->m_simpleSelector)
                        break;
                    if (!checkOneSelector(subSel, e, selectorAttrs, dynamicPseudo, isAncestor, true, elementStyle, elementParentStyle))
                        return true;
                }
                break;
            }
            case CSSSelector::PseudoUnknown:
            case CSSSelector::PseudoNotParsed:
            default:
                ASSERT_NOT_REACHED();
                break;
        }
        return false;
    }
    if (sel->m_match == CSSSelector::PseudoElement) {
        if (!elementStyle)
            return false;

        switch (sel->pseudoType()) {
            // Pseudo-elements:
            case CSSSelector::PseudoFirstLine:
                dynamicPseudo = RenderStyle::FIRST_LINE;
                return true;
            case CSSSelector::PseudoFirstLetter:
                dynamicPseudo = RenderStyle::FIRST_LETTER;
                if (Document* doc = e->document())
                    doc->setUsesFirstLetterRules(true);
                return true;
            case CSSSelector::PseudoSelection:
                dynamicPseudo = RenderStyle::SELECTION;
                return true;
            case CSSSelector::PseudoBefore:
                dynamicPseudo = RenderStyle::BEFORE;
                return true;
            case CSSSelector::PseudoAfter:
                dynamicPseudo = RenderStyle::AFTER;
                return true;
            case CSSSelector::PseudoFileUploadButton:
                dynamicPseudo = RenderStyle::FILE_UPLOAD_BUTTON;
                return true;
            case CSSSelector::PseudoSliderThumb:
                dynamicPseudo = RenderStyle::SLIDER_THUMB;
                return true; 
            case CSSSelector::PseudoSearchCancelButton:
                dynamicPseudo = RenderStyle::SEARCH_CANCEL_BUTTON;
                return true; 
            case CSSSelector::PseudoSearchDecoration:
                dynamicPseudo = RenderStyle::SEARCH_DECORATION;
                return true;
            case CSSSelector::PseudoSearchResultsDecoration:
                dynamicPseudo = RenderStyle::SEARCH_RESULTS_DECORATION;
                return true;
            case CSSSelector::PseudoSearchResultsButton:
                dynamicPseudo = RenderStyle::SEARCH_RESULTS_BUTTON;
                return true;
            case CSSSelector::PseudoMediaControlsPanel:
                dynamicPseudo = RenderStyle::MEDIA_CONTROLS_PANEL;
                return true;
            case CSSSelector::PseudoMediaControlsMuteButton:
                dynamicPseudo = RenderStyle::MEDIA_CONTROLS_MUTE_BUTTON;
                return true;
            case CSSSelector::PseudoMediaControlsPlayButton:
                dynamicPseudo = RenderStyle::MEDIA_CONTROLS_PLAY_BUTTON;
                return true;
            case CSSSelector::PseudoMediaControlsTimeDisplay:
                dynamicPseudo = RenderStyle::MEDIA_CONTROLS_TIME_DISPLAY;
                return true;
            case CSSSelector::PseudoMediaControlsTimeline:
                dynamicPseudo = RenderStyle::MEDIA_CONTROLS_TIMELINE;
                return true;
            case CSSSelector::PseudoMediaControlsSeekBackButton:
                dynamicPseudo = RenderStyle::MEDIA_CONTROLS_SEEK_BACK_BUTTON;
                return true;
            case CSSSelector::PseudoMediaControlsSeekForwardButton:
                dynamicPseudo = RenderStyle::MEDIA_CONTROLS_SEEK_FORWARD_BUTTON;
                return true;
            case CSSSelector::PseudoMediaControlsFullscreenButton:
                dynamicPseudo = RenderStyle::MEDIA_CONTROLS_FULLSCREEN_BUTTON;
                return true;
            case CSSSelector::PseudoUnknown:
            case CSSSelector::PseudoNotParsed:
            default:
                ASSERT_NOT_REACHED();
                break;
        }
        return false;
    }
    // ### add the rest of the checks...
    return true;
}

void CSSStyleSelector::addVariables(CSSVariablesRule* variables)
{
    CSSVariablesDeclaration* decl = variables->variables();
    if (!decl)
        return;
    unsigned size = decl->length();
    for (unsigned i = 0; i < size; ++i) {
        String name = decl->item(i);
        m_variablesMap.set(name, variables);
    }
}

CSSValue* CSSStyleSelector::resolveVariableDependentValue(CSSVariableDependentValue* val)
{
    return 0;
}

// -----------------------------------------------------------------

CSSRuleSet::CSSRuleSet()
{
    m_universalRules = 0;
    m_ruleCount = 0;
}

CSSRuleSet::~CSSRuleSet()
{ 
    deleteAllValues(m_idRules);
    deleteAllValues(m_classRules);
    deleteAllValues(m_tagRules);

    delete m_universalRules; 
}


void CSSRuleSet::addToRuleSet(AtomicStringImpl* key, AtomRuleMap& map,
                              CSSStyleRule* rule, CSSSelector* sel)
{
    if (!key) return;
    CSSRuleDataList* rules = map.get(key);
    if (!rules) {
        rules = new CSSRuleDataList(m_ruleCount++, rule, sel);
        map.set(key, rules);
    } else
        rules->append(m_ruleCount++, rule, sel);
}

void CSSRuleSet::addRule(CSSStyleRule* rule, CSSSelector* sel)
{
    if (sel->m_match == CSSSelector::Id) {
        addToRuleSet(sel->m_value.impl(), m_idRules, rule, sel);
        return;
    }
    if (sel->m_match == CSSSelector::Class) {
        addToRuleSet(sel->m_value.impl(), m_classRules, rule, sel);
        return;
    }
     
    const AtomicString& localName = sel->m_tag.localName();
    if (localName != starAtom) {
        addToRuleSet(localName.impl(), m_tagRules, rule, sel);
        return;
    }
    
    // Just put it in the universal rule set.
    if (!m_universalRules)
        m_universalRules = new CSSRuleDataList(m_ruleCount++, rule, sel);
    else
        m_universalRules->append(m_ruleCount++, rule, sel);
}

void CSSRuleSet::addRulesFromSheet(CSSStyleSheet* sheet, const MediaQueryEvaluator& medium, CSSStyleSelector* styleSelector)
{
    if (!sheet)
        return;

    // No media implies "all", but if a media list exists it must
    // contain our current medium
    if (sheet->media() && !medium.eval(sheet->media(), styleSelector))
        return; // the style sheet doesn't apply

    int len = sheet->length();

    for (int i = 0; i < len; i++) {
        StyleBase* item = sheet->item(i);
        if (item->isStyleRule()) {
            CSSStyleRule* rule = static_cast<CSSStyleRule*>(item);
            for (CSSSelector* s = rule->selector(); s; s = s->next())
                addRule(rule, s);
        }
        else if (item->isImportRule()) {
            CSSImportRule* import = static_cast<CSSImportRule*>(item);
            if (!import->media() || medium.eval(import->media(), styleSelector))
                addRulesFromSheet(import->styleSheet(), medium, styleSelector);
        }
        else if (item->isMediaRule()) {
            CSSMediaRule* r = static_cast<CSSMediaRule*>(item);
            CSSRuleList* rules = r->cssRules();

            if ((!r->media() || medium.eval(r->media(), styleSelector)) && rules) {
                // Traverse child elements of the @media rule.
                for (unsigned j = 0; j < rules->length(); j++) {
                    CSSRule *childItem = rules->item(j);
                    if (childItem->isStyleRule()) {
                        // It is a StyleRule, so append it to our list
                        CSSStyleRule* rule = static_cast<CSSStyleRule*>(childItem);
                        for (CSSSelector* s = rule->selector(); s; s = s->next())
                            addRule(rule, s);
                    } else if (item->isFontFaceRule() && styleSelector) {
                        // Add this font face to our set.
                        const CSSFontFaceRule* fontFaceRule = static_cast<CSSFontFaceRule*>(item);
                        styleSelector->fontSelector()->addFontFaceRule(fontFaceRule);
                    }
                }   // for rules
            }   // if rules
        } else if (item->isFontFaceRule() && styleSelector) {
            // Add this font face to our set.
            const CSSFontFaceRule* fontFaceRule = static_cast<CSSFontFaceRule*>(item);
            styleSelector->fontSelector()->addFontFaceRule(fontFaceRule);
        } else if (item->isVariablesRule()) {
            // Evaluate the media query and make sure it matches.
            CSSVariablesRule* variables = static_cast<CSSVariablesRule*>(item);
            if (!variables->media() || medium.eval(variables->media(), styleSelector))
                styleSelector->addVariables(variables);
        }
    }
}

// -------------------------------------------------------------------------------------
// this is mostly boring stuff on how to apply a certain rule to the renderstyle...

static Length convertToLength(CSSPrimitiveValue *primitiveValue, RenderStyle *style, bool *ok = 0)
{
    Length l;
    if (!primitiveValue) {
        if (ok)
            *ok = false;
    } else {
        int type = primitiveValue->primitiveType();
        if (type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG)
            l = Length(primitiveValue->computeLengthIntForLength(style), Fixed);
        else if (type == CSSPrimitiveValue::CSS_PERCENTAGE)
            l = Length(primitiveValue->getDoubleValue(), Percent);
        else if (type == CSSPrimitiveValue::CSS_NUMBER)
            l = Length(primitiveValue->getDoubleValue() * 100.0, Percent);
        else if (ok)
            *ok = false;
    }
    return l;
}

void CSSStyleSelector::applyDeclarations(bool applyFirst, bool isImportant,
                                         int startIndex, int endIndex)
{
    if (startIndex == -1)
        return;

    for (int i = startIndex; i <= endIndex; i++) {
        CSSMutableStyleDeclaration* decl = m_matchedDecls[i];
        DeprecatedValueListConstIterator<CSSProperty> end;
        for (DeprecatedValueListConstIterator<CSSProperty> it = decl->valuesIterator(); it != end; ++it) {
            const CSSProperty& current = *it;
            // give special priority to font-xxx, color properties
            if (isImportant == current.isImportant()) {
                bool first;
                switch (current.id()) {
                    case CSSPropertyLineHeight:
                        m_lineHeightValue = current.value();
                        first = !applyFirst; // we apply line-height later
                        break;
                    case CSSPropertyColor:
                    case CSSPropertyDirection:
                    case CSSPropertyDisplay:
                    case CSSPropertyFont:
                    case CSSPropertyFontSize:
                    case CSSPropertyFontStyle:
                    case CSSPropertyFontFamily:
                    case CSSPropertyFontWeight:
                    case CSSPropertyWebkitTextSizeAdjust:
                    case CSSPropertyFontVariant:
                    case CSSPropertyZoom:
                        // these have to be applied first, because other properties use the computed
                        // values of these porperties.
                        first = true;
                        break;
                    default:
                        first = false;
                        break;
                }
                if (first == applyFirst)
                    applyProperty(current.id(), current.value());
            }
        }
    }
}

static void applyCounterList(RenderStyle* style, CSSValueList* list, bool isReset)
{
    CounterDirectiveMap& map = style->accessCounterDirectives();
    typedef CounterDirectiveMap::iterator Iterator;

    Iterator end = map.end();
    for (Iterator it = map.begin(); it != end; ++it)
        if (isReset)
            it->second.m_reset = false;
        else
            it->second.m_increment = false;

    int length = list ? list->length() : 0;
    for (int i = 0; i < length; ++i) {
        Pair* pair = static_cast<CSSPrimitiveValue*>(list->itemWithoutBoundsCheck(i))->getPairValue();
        AtomicString identifier = static_cast<CSSPrimitiveValue*>(pair->first())->getStringValue();
        // FIXME: What about overflow?
        int value = static_cast<CSSPrimitiveValue*>(pair->second())->getIntValue();
        CounterDirectives& directives = map.add(identifier.impl(), CounterDirectives()).first->second;
        if (isReset) {
            directives.m_reset = true;
            directives.m_resetValue = value;
        } else {
            if (directives.m_increment)
                directives.m_incrementValue += value;
            else {
                directives.m_increment = true;
                directives.m_incrementValue = value;
            }
        }
    }
}

void CSSStyleSelector::applyProperty(int id, CSSValue *value)
{
    CSSPrimitiveValue* primitiveValue = 0;
    if (value->isPrimitiveValue())
        primitiveValue = static_cast<CSSPrimitiveValue*>(value);

    float zoomFactor = m_style->effectiveZoom();

    Length l;
    bool apply = false;

    unsigned short valueType = value->cssValueType();

    bool isInherit = m_parentNode && valueType == CSSValue::CSS_INHERIT;
    bool isInitial = valueType == CSSValue::CSS_INITIAL || (!m_parentNode && valueType == CSSValue::CSS_INHERIT);
   
    // These properties are used to set the correct margins/padding on RTL lists.
    if (id == CSSPropertyWebkitMarginStart)
        id = m_style->direction() == LTR ? CSSPropertyMarginLeft : CSSPropertyMarginRight;
    else if (id == CSSPropertyWebkitPaddingStart)
        id = m_style->direction() == LTR ? CSSPropertyPaddingLeft : CSSPropertyPaddingRight;

    // What follows is a list that maps the CSS properties into their corresponding front-end
    // RenderStyle values.  Shorthands (e.g. border, background) occur in this list as well and
    // are only hit when mapping "inherit" or "initial" into front-end values.
    switch (static_cast<CSSPropertyID>(id)) {
// ident only properties
    case CSSPropertyBackgroundAttachment:
        HANDLE_BACKGROUND_VALUE(attachment, Attachment, value)
        return;
    case CSSPropertyWebkitBackgroundClip:
        HANDLE_BACKGROUND_VALUE(clip, Clip, value)
        return;
    case CSSPropertyWebkitBackgroundComposite:
        HANDLE_BACKGROUND_VALUE(composite, Composite, value)
        return;
    case CSSPropertyWebkitBackgroundOrigin:
        HANDLE_BACKGROUND_VALUE(origin, Origin, value)
        return;
    case CSSPropertyBackgroundRepeat:
        HANDLE_BACKGROUND_VALUE(repeat, Repeat, value)
        return;
    case CSSPropertyWebkitBackgroundSize:
        HANDLE_BACKGROUND_VALUE(size, Size, value)
        return;
    case CSSPropertyWebkitMaskAttachment:
        HANDLE_MASK_VALUE(attachment, Attachment, value)
        return;
    case CSSPropertyWebkitMaskClip:
        HANDLE_MASK_VALUE(clip, Clip, value)
        return;
    case CSSPropertyWebkitMaskComposite:
        HANDLE_MASK_VALUE(composite, Composite, value)
        return;
    case CSSPropertyWebkitMaskOrigin:
        HANDLE_MASK_VALUE(origin, Origin, value)
        return;
    case CSSPropertyWebkitMaskRepeat:
        HANDLE_MASK_VALUE(repeat, Repeat, value)
        return;
    case CSSPropertyWebkitMaskSize:
        HANDLE_MASK_VALUE(size, Size, value)
        return;
    case CSSPropertyBorderCollapse:
        HANDLE_INHERIT_AND_INITIAL(borderCollapse, BorderCollapse)
        if (!primitiveValue)
            return;
        switch (primitiveValue->getIdent()) {
            case CSSValueCollapse:
                m_style->setBorderCollapse(true);
                break;
            case CSSValueSeparate:
                m_style->setBorderCollapse(false);
                break;
            default:
                return;
        }
        return;
       
    case CSSPropertyBorderTopStyle:
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(borderTopStyle, BorderTopStyle, BorderStyle)
        if (primitiveValue)
            m_style->setBorderTopStyle(*primitiveValue);
        return;
    case CSSPropertyBorderRightStyle:
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(borderRightStyle, BorderRightStyle, BorderStyle)
        if (primitiveValue)
            m_style->setBorderRightStyle(*primitiveValue);
        return;
    case CSSPropertyBorderBottomStyle:
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(borderBottomStyle, BorderBottomStyle, BorderStyle)
        if (primitiveValue)
            m_style->setBorderBottomStyle(*primitiveValue);
        return;
    case CSSPropertyBorderLeftStyle:
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(borderLeftStyle, BorderLeftStyle, BorderStyle)
        if (primitiveValue)
            m_style->setBorderLeftStyle(*primitiveValue);
        return;
    case CSSPropertyOutlineStyle:
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(outlineStyle, OutlineStyle, BorderStyle)
        if (primitiveValue) {
            if (primitiveValue->getIdent() == CSSValueAuto)
                m_style->setOutlineStyle(DOTTED, true);
            else
                m_style->setOutlineStyle(*primitiveValue);
        }
        return;
    case CSSPropertyCaptionSide:
    {
        HANDLE_INHERIT_AND_INITIAL(captionSide, CaptionSide)
        if (primitiveValue)
            m_style->setCaptionSide(*primitiveValue);
        return;
    }
    case CSSPropertyClear:
    {
        HANDLE_INHERIT_AND_INITIAL(clear, Clear)
        if (primitiveValue)
            m_style->setClear(*primitiveValue);
        return;
    }
    case CSSPropertyDirection:
    {
        HANDLE_INHERIT_AND_INITIAL(direction, Direction)
        if (primitiveValue)
            m_style->setDirection(*primitiveValue);
        return;
    }
    case CSSPropertyDisplay:
    {
        HANDLE_INHERIT_AND_INITIAL(display, Display)
        if (primitiveValue)
            m_style->setDisplay(*primitiveValue);
        return;
    }

    case CSSPropertyEmptyCells:
    {
        HANDLE_INHERIT_AND_INITIAL(emptyCells, EmptyCells)
        if (primitiveValue)
            m_style->setEmptyCells(*primitiveValue);
        return;
    }
    case CSSPropertyFloat:
    {
        HANDLE_INHERIT_AND_INITIAL(floating, Floating)
        if (primitiveValue)
            m_style->setFloating(*primitiveValue);
        return;
    }

    case CSSPropertyFontStyle:
    {
        FontDescription fontDescription = m_style->fontDescription();
        if (isInherit)
            fontDescription.setItalic(m_parentStyle->fontDescription().italic());
        else if (isInitial)
            fontDescription.setItalic(false);
        else {
            if (!primitiveValue)
                return;
            switch (primitiveValue->getIdent()) {
                case CSSValueOblique:
                // FIXME: oblique is the same as italic for the moment...
                case CSSValueItalic:
                    fontDescription.setItalic(true);
                    break;
                case CSSValueNormal:
                    fontDescription.setItalic(false);
                    break;
                default:
                    return;
            }
        }
        if (m_style->setFontDescription(fontDescription))
            m_fontDirty = true;
        return;
    }

    case CSSPropertyFontVariant:
    {
        FontDescription fontDescription = m_style->fontDescription();
        if (isInherit) 
            fontDescription.setSmallCaps(m_parentStyle->fontDescription().smallCaps());
        else if (isInitial)
            fontDescription.setSmallCaps(false);
        else {
            if (!primitiveValue)
                return;
            int id = primitiveValue->getIdent();
            if (id == CSSValueNormal)
                fontDescription.setSmallCaps(false);
            else if (id == CSSValueSmallCaps)
                fontDescription.setSmallCaps(true);
            else
                return;
        }
        if (m_style->setFontDescription(fontDescription))
            m_fontDirty = true;
        return;        
    }

    case CSSPropertyFontWeight:
    {
        FontDescription fontDescription = m_style->fontDescription();
        if (isInherit)
            fontDescription.setWeight(m_parentStyle->fontDescription().weight());
        else if (isInitial)
            fontDescription.setWeight(FontWeightNormal);
        else {
            if (!primitiveValue)
                return;
            if (primitiveValue->getIdent()) {
                switch (primitiveValue->getIdent()) {
                    case CSSValueBolder:
                        fontDescription.setWeight(fontDescription.bolderWeight());
                        break;
                    case CSSValueLighter:
                        fontDescription.setWeight(fontDescription.lighterWeight());
                        break;
                    case CSSValueBold:
                    case CSSValue700:
                        fontDescription.setWeight(FontWeightBold);
                        break;
                    case CSSValueNormal:
                    case CSSValue400:
                        fontDescription.setWeight(FontWeightNormal);
                        break;
                    case CSSValue900:
                        fontDescription.setWeight(FontWeight900);
                        break;
                    case CSSValue800:
                        fontDescription.setWeight(FontWeight800);
                        break;
                    case CSSValue600:
                        fontDescription.setWeight(FontWeight600);
                        break;
                    case CSSValue500:
                        fontDescription.setWeight(FontWeight500);
                        break;
                    case CSSValue300:
                        fontDescription.setWeight(FontWeight300);
                        break;
                    case CSSValue200:
                        fontDescription.setWeight(FontWeight200);
                        break;
                    case CSSValue100:
                        fontDescription.setWeight(FontWeight100);
                        break;
                    default:
                        return;
                }
            } else
                ASSERT_NOT_REACHED();
        }
        if (m_style->setFontDescription(fontDescription))
            m_fontDirty = true;
        return;
    }
        
    case CSSPropertyListStylePosition:
    {
        HANDLE_INHERIT_AND_INITIAL(listStylePosition, ListStylePosition)
        if (primitiveValue)
            m_style->setListStylePosition(*primitiveValue);
        return;
    }

    case CSSPropertyListStyleType:
    {
        HANDLE_INHERIT_AND_INITIAL(listStyleType, ListStyleType)
        if (primitiveValue)
            m_style->setListStyleType(*primitiveValue);
        return;
    }

    case CSSPropertyOverflow:
    {
        if (isInherit) {
            m_style->setOverflowX(m_parentStyle->overflowX());
            m_style->setOverflowY(m_parentStyle->overflowY());
            return;
        }
        
        if (isInitial) {
            m_style->setOverflowX(RenderStyle::initialOverflowX());
            m_style->setOverflowY(RenderStyle::initialOverflowY());
            return;
        }
            
        EOverflow o = *primitiveValue;

        m_style->setOverflowX(o);
        m_style->setOverflowY(o);
        return;
    }

    case CSSPropertyOverflowX:
    {
        HANDLE_INHERIT_AND_INITIAL(overflowX, OverflowX)
        m_style->setOverflowX(*primitiveValue);
        return;
    }

    case CSSPropertyOverflowY:
    {
        HANDLE_INHERIT_AND_INITIAL(overflowY, OverflowY)
        m_style->setOverflowY(*primitiveValue);
        return;
    }

    case CSSPropertyPageBreakBefore:
    {
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(pageBreakBefore, PageBreakBefore, PageBreak)
        if (primitiveValue)
            m_style->setPageBreakBefore(*primitiveValue);
        return;
    }

    case CSSPropertyPageBreakAfter:
    {
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(pageBreakAfter, PageBreakAfter, PageBreak)
        if (primitiveValue)
            m_style->setPageBreakAfter(*primitiveValue);
        return;
    }

    case CSSPropertyPageBreakInside: {
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(pageBreakInside, PageBreakInside, PageBreak)
        if (!primitiveValue)
            return;
        EPageBreak pageBreak = *primitiveValue;
        if (pageBreak != PBALWAYS)
            m_style->setPageBreakInside(pageBreak);
        return;
    }
        
    case CSSPropertyPosition:
    {
        HANDLE_INHERIT_AND_INITIAL(position, Position)
        if (primitiveValue)
            m_style->setPosition(*primitiveValue);
        return;
    }

    case CSSPropertyTableLayout: {
        HANDLE_INHERIT_AND_INITIAL(tableLayout, TableLayout)

        ETableLayout l = *primitiveValue;
        if (l == TAUTO)
            l = RenderStyle::initialTableLayout();

        m_style->setTableLayout(l);
        return;
    }
        
    case CSSPropertyUnicodeBidi: {
        HANDLE_INHERIT_AND_INITIAL(unicodeBidi, UnicodeBidi)
        m_style->setUnicodeBidi(*primitiveValue);
        return;
    }
    case CSSPropertyTextTransform: {
        HANDLE_INHERIT_AND_INITIAL(textTransform, TextTransform)
        m_style->setTextTransform(*primitiveValue);
        return;
    }

    case CSSPropertyVisibility:
    {
        HANDLE_INHERIT_AND_INITIAL(visibility, Visibility)
        m_style->setVisibility(*primitiveValue);
        return;
    }
    case CSSPropertyWhiteSpace:
        HANDLE_INHERIT_AND_INITIAL(whiteSpace, WhiteSpace)
        m_style->setWhiteSpace(*primitiveValue);
        return;

    case CSSPropertyBackgroundPosition:
        HANDLE_BACKGROUND_INHERIT_AND_INITIAL(xPosition, XPosition);
        HANDLE_BACKGROUND_INHERIT_AND_INITIAL(yPosition, YPosition);
        return;
    case CSSPropertyBackgroundPositionX: {
        HANDLE_BACKGROUND_VALUE(xPosition, XPosition, value)
        return;
    }
    case CSSPropertyBackgroundPositionY: {
        HANDLE_BACKGROUND_VALUE(yPosition, YPosition, value)
        return;
    }
    case CSSPropertyWebkitMaskPosition:
        HANDLE_MASK_INHERIT_AND_INITIAL(xPosition, XPosition);
        HANDLE_MASK_INHERIT_AND_INITIAL(yPosition, YPosition);
        return;
    case CSSPropertyWebkitMaskPositionX: {
        HANDLE_MASK_VALUE(xPosition, XPosition, value)
        return;
    }
    case CSSPropertyWebkitMaskPositionY: {
        HANDLE_MASK_VALUE(yPosition, YPosition, value)
        return;
    }
    case CSSPropertyBorderSpacing: {
        if (isInherit) {
            m_style->setHorizontalBorderSpacing(m_parentStyle->horizontalBorderSpacing());
            m_style->setVerticalBorderSpacing(m_parentStyle->verticalBorderSpacing());
        }
        else if (isInitial) {
            m_style->setHorizontalBorderSpacing(0);
            m_style->setVerticalBorderSpacing(0);
        }
        return;
    }
    case CSSPropertyWebkitBorderHorizontalSpacing: {
        HANDLE_INHERIT_AND_INITIAL(horizontalBorderSpacing, HorizontalBorderSpacing)
        if (!primitiveValue)
            return;
        short spacing =  primitiveValue->computeLengthShort(style(), zoomFactor);
        m_style->setHorizontalBorderSpacing(spacing);
        return;
    }
    case CSSPropertyWebkitBorderVerticalSpacing: {
        HANDLE_INHERIT_AND_INITIAL(verticalBorderSpacing, VerticalBorderSpacing)
        if (!primitiveValue)
            return;
        short spacing =  primitiveValue->computeLengthShort(style(), zoomFactor);
        m_style->setVerticalBorderSpacing(spacing);
        return;
    }
    case CSSPropertyCursor:
        if (isInherit) {
            m_style->setCursor(m_parentStyle->cursor());
            m_style->setCursorList(m_parentStyle->cursors());
            return;
        }
        m_style->clearCursorList();
        if (isInitial) {
            m_style->setCursor(RenderStyle::initialCursor());
            return;
        }
        if (value->isValueList()) {
            CSSValueList* list = static_cast<CSSValueList*>(value);
            int len = list->length();
            m_style->setCursor(CURSOR_AUTO);
            for (int i = 0; i < len; i++) {
                CSSValue* item = list->itemWithoutBoundsCheck(i);
                if (!item->isPrimitiveValue())
                    continue;
                primitiveValue = static_cast<CSSPrimitiveValue*>(item);
                int type = primitiveValue->primitiveType();
                if (type == CSSPrimitiveValue::CSS_URI) {
                    CSSCursorImageValue* image = static_cast<CSSCursorImageValue*>(primitiveValue);
                    if (image->updateIfSVGCursorIsUsed(m_element)) // Elements with SVG cursors are not allowed to share style.
                        m_style->setUnique();
                    // FIXME: Temporary clumsiness to pass off a CachedImage to an API that will eventually convert to using
                    // StyleImage.
                    RefPtr<StyleCachedImage> styleCachedImage(image->cachedImage(m_element->document()->docLoader()));
                    if (styleCachedImage)
                        m_style->addCursor(styleCachedImage->cachedImage(), image->hotspot());
                } else if (type == CSSPrimitiveValue::CSS_IDENT)
                    m_style->setCursor(*primitiveValue);
            }
        } else if (primitiveValue) {
            int type = primitiveValue->primitiveType();
            if (type == CSSPrimitiveValue::CSS_IDENT)
                m_style->setCursor(*primitiveValue);
        }
        return;
// colors || inherit
    case CSSPropertyColor:
        // If the 'currentColor' keyword is set on the 'color' property itself,
        // it is treated as 'color:inherit' at parse time
        if (primitiveValue && primitiveValue->getIdent() == CSSValueCurrentcolor)
            isInherit = true;
    case CSSPropertyBackgroundColor:
    case CSSPropertyBorderTopColor:
    case CSSPropertyBorderRightColor:
    case CSSPropertyBorderBottomColor:
    case CSSPropertyBorderLeftColor:
    case CSSPropertyOutlineColor:
    case CSSPropertyWebkitColumnRuleColor:
    case CSSPropertyWebkitTextStrokeColor:
    case CSSPropertyWebkitTextFillColor: {
        Color col;
        if (isInherit) {
            HANDLE_INHERIT_COND(CSSPropertyBackgroundColor, backgroundColor, BackgroundColor)
            HANDLE_INHERIT_COND_WITH_BACKUP(CSSPropertyBorderTopColor, borderTopColor, color, BorderTopColor)
            HANDLE_INHERIT_COND_WITH_BACKUP(CSSPropertyBorderBottomColor, borderBottomColor, color, BorderBottomColor)
            HANDLE_INHERIT_COND_WITH_BACKUP(CSSPropertyBorderRightColor, borderRightColor, color, BorderRightColor)
            HANDLE_INHERIT_COND_WITH_BACKUP(CSSPropertyBorderLeftColor, borderLeftColor, color, BorderLeftColor)
            HANDLE_INHERIT_COND(CSSPropertyColor, color, Color)
            HANDLE_INHERIT_COND_WITH_BACKUP(CSSPropertyOutlineColor, outlineColor, color, OutlineColor)
            HANDLE_INHERIT_COND_WITH_BACKUP(CSSPropertyWebkitColumnRuleColor, columnRuleColor, color, ColumnRuleColor)
            HANDLE_INHERIT_COND_WITH_BACKUP(CSSPropertyWebkitTextStrokeColor, textStrokeColor, color, TextStrokeColor)
            HANDLE_INHERIT_COND_WITH_BACKUP(CSSPropertyWebkitTextFillColor, textFillColor, color, TextFillColor)
            return;
        }
        if (isInitial) {
            // The border/outline colors will just map to the invalid color |col| above.  This will have the
            // effect of forcing the use of the currentColor when it comes time to draw the borders (and of
            // not painting the background since the color won't be valid).
            if (id == CSSPropertyColor)
                col = RenderStyle::initialColor();
        } else {
            if (!primitiveValue)
                return;
            col = getColorFromPrimitiveValue(primitiveValue);
        }

        switch (id) {
        case CSSPropertyBackgroundColor:
            m_style->setBackgroundColor(col);
            break;
        case CSSPropertyBorderTopColor:
            m_style->setBorderTopColor(col);
            break;
        case CSSPropertyBorderRightColor:
            m_style->setBorderRightColor(col);
            break;
        case CSSPropertyBorderBottomColor:
            m_style->setBorderBottomColor(col);
            break;
        case CSSPropertyBorderLeftColor:
            m_style->setBorderLeftColor(col);
            break;
        case CSSPropertyColor:
            m_style->setColor(col);
            break;
        case CSSPropertyOutlineColor:
            m_style->setOutlineColor(col);
            break;
        case CSSPropertyWebkitColumnRuleColor:
            m_style->setColumnRuleColor(col);
            break;
        case CSSPropertyWebkitTextStrokeColor:
            m_style->setTextStrokeColor(col);
            break;
        case CSSPropertyWebkitTextFillColor:
            m_style->setTextFillColor(col);
            break;
        }
        
        return;
    }
    
// uri || inherit
    case CSSPropertyBackgroundImage:
        HANDLE_BACKGROUND_VALUE(image, Image, value)
        return;
    case CSSPropertyWebkitMaskImage:
        HANDLE_MASK_VALUE(image, Image, value)
        return;
    case CSSPropertyListStyleImage:
    {
        HANDLE_INHERIT_AND_INITIAL(listStyleImage, ListStyleImage)
        m_style->setListStyleImage(styleImage(value));
        return;
    }

// length
    case CSSPropertyBorderTopWidth:
    case CSSPropertyBorderRightWidth:
    case CSSPropertyBorderBottomWidth:
    case CSSPropertyBorderLeftWidth:
    case CSSPropertyOutlineWidth:
    case CSSPropertyWebkitColumnRuleWidth:
    {
        if (isInherit) {
            HANDLE_INHERIT_COND(CSSPropertyBorderTopWidth, borderTopWidth, BorderTopWidth)
            HANDLE_INHERIT_COND(CSSPropertyBorderRightWidth, borderRightWidth, BorderRightWidth)
            HANDLE_INHERIT_COND(CSSPropertyBorderBottomWidth, borderBottomWidth, BorderBottomWidth)
            HANDLE_INHERIT_COND(CSSPropertyBorderLeftWidth, borderLeftWidth, BorderLeftWidth)
            HANDLE_INHERIT_COND(CSSPropertyOutlineWidth, outlineWidth, OutlineWidth)
            HANDLE_INHERIT_COND(CSSPropertyWebkitColumnRuleWidth, columnRuleWidth, ColumnRuleWidth)
            return;
        }
        else if (isInitial) {
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyBorderTopWidth, BorderTopWidth, BorderWidth)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyBorderRightWidth, BorderRightWidth, BorderWidth)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyBorderBottomWidth, BorderBottomWidth, BorderWidth)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyBorderLeftWidth, BorderLeftWidth, BorderWidth)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyOutlineWidth, OutlineWidth, BorderWidth)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyWebkitColumnRuleWidth, ColumnRuleWidth, BorderWidth)
            return;
        }

        if (!primitiveValue)
            return;
        short width = 3;
        switch (primitiveValue->getIdent()) {
        case CSSValueThin:
            width = 1;
            break;
        case CSSValueMedium:
            width = 3;
            break;
        case CSSValueThick:
            width = 5;
            break;
        case CSSValueInvalid:
            width = primitiveValue->computeLengthShort(style(), zoomFactor);
            break;
        default:
            return;
        }

        if (width < 0) return;
        switch (id) {
        case CSSPropertyBorderTopWidth:
            m_style->setBorderTopWidth(width);
            break;
        case CSSPropertyBorderRightWidth:
            m_style->setBorderRightWidth(width);
            break;
        case CSSPropertyBorderBottomWidth:
            m_style->setBorderBottomWidth(width);
            break;
        case CSSPropertyBorderLeftWidth:
            m_style->setBorderLeftWidth(width);
            break;
        case CSSPropertyOutlineWidth:
            m_style->setOutlineWidth(width);
            break;
        case CSSPropertyWebkitColumnRuleWidth:
            m_style->setColumnRuleWidth(width);
            break;
        default:
            return;
        }
        return;
    }

    case CSSPropertyLetterSpacing:
    case CSSPropertyWordSpacing:
    {
        
        if (isInherit) {
            HANDLE_INHERIT_COND(CSSPropertyLetterSpacing, letterSpacing, LetterSpacing)
            HANDLE_INHERIT_COND(CSSPropertyWordSpacing, wordSpacing, WordSpacing)
            return;
        }
        else if (isInitial) {
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyLetterSpacing, LetterSpacing, LetterWordSpacing)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyWordSpacing, WordSpacing, LetterWordSpacing)
            return;
        }
        
        int width = 0;
        if (primitiveValue && primitiveValue->getIdent() == CSSValueNormal){
            width = 0;
        } else {
            if (!primitiveValue)
                return;
            width = primitiveValue->computeLengthInt(style(), zoomFactor);
        }
        switch (id) {
        case CSSPropertyLetterSpacing:
            m_style->setLetterSpacing(width);
            break;
        case CSSPropertyWordSpacing:
            m_style->setWordSpacing(width);
            break;
            // ### needs the definitions in renderstyle
        default: break;
        }
        return;
    }

    case CSSPropertyWordBreak: {
        HANDLE_INHERIT_AND_INITIAL(wordBreak, WordBreak)
        m_style->setWordBreak(*primitiveValue);
        return;
    }

    case CSSPropertyWordWrap: {
        HANDLE_INHERIT_AND_INITIAL(wordWrap, WordWrap)
        m_style->setWordWrap(*primitiveValue);
        return;
    }

    case CSSPropertyWebkitNbspMode:
    {
        HANDLE_INHERIT_AND_INITIAL(nbspMode, NBSPMode)
        m_style->setNBSPMode(*primitiveValue);
        return;
    }

    case CSSPropertyWebkitLineBreak:
    {
        HANDLE_INHERIT_AND_INITIAL(khtmlLineBreak, KHTMLLineBreak)
        m_style->setKHTMLLineBreak(*primitiveValue);
        return;
    }

    case CSSPropertyWebkitMatchNearestMailBlockquoteColor:
    {
        HANDLE_INHERIT_AND_INITIAL(matchNearestMailBlockquoteColor, MatchNearestMailBlockquoteColor)
        m_style->setMatchNearestMailBlockquoteColor(*primitiveValue);
        return;
    }

    case CSSPropertyResize:
    {
        HANDLE_INHERIT_AND_INITIAL(resize, Resize)

        if (!primitiveValue->getIdent())
            return;

        EResize r = RESIZE_NONE;
        if (primitiveValue->getIdent() == CSSValueAuto) {
            if (Settings* settings = m_checker.m_document->settings())
                r = settings->textAreasAreResizable() ? RESIZE_BOTH : RESIZE_NONE;
        } else
            r = *primitiveValue;
            
        m_style->setResize(r);
        return;
    }
    
    // length, percent
    case CSSPropertyMaxWidth:
        // +none +inherit
        if (primitiveValue && primitiveValue->getIdent() == CSSValueNone)
            apply = true;
    case CSSPropertyTop:
    case CSSPropertyLeft:
    case CSSPropertyRight:
    case CSSPropertyBottom:
    case CSSPropertyWidth:
    case CSSPropertyMinWidth:
    case CSSPropertyMarginTop:
    case CSSPropertyMarginRight:
    case CSSPropertyMarginBottom:
    case CSSPropertyMarginLeft:
        // +inherit +auto
        if (id == CSSPropertyWidth || id == CSSPropertyMinWidth || id == CSSPropertyMaxWidth) {
            if (primitiveValue && primitiveValue->getIdent() == CSSValueIntrinsic) {
                l = Length(Intrinsic);
                apply = true;
            }
            else if (primitiveValue && primitiveValue->getIdent() == CSSValueMinIntrinsic) {
                l = Length(MinIntrinsic);
                apply = true;
            }
        }
        if (id != CSSPropertyMaxWidth && primitiveValue && primitiveValue->getIdent() == CSSValueAuto)
            apply = true;
    case CSSPropertyPaddingTop:
    case CSSPropertyPaddingRight:
    case CSSPropertyPaddingBottom:
    case CSSPropertyPaddingLeft:
    case CSSPropertyTextIndent:
        // +inherit
    {
        if (isInherit) {
            HANDLE_INHERIT_COND(CSSPropertyMaxWidth, maxWidth, MaxWidth)
            HANDLE_INHERIT_COND(CSSPropertyBottom, bottom, Bottom)
            HANDLE_INHERIT_COND(CSSPropertyTop, top, Top)
            HANDLE_INHERIT_COND(CSSPropertyLeft, left, Left)
            HANDLE_INHERIT_COND(CSSPropertyRight, right, Right)
            HANDLE_INHERIT_COND(CSSPropertyWidth, width, Width)
            HANDLE_INHERIT_COND(CSSPropertyMinWidth, minWidth, MinWidth)
            HANDLE_INHERIT_COND(CSSPropertyPaddingTop, paddingTop, PaddingTop)
            HANDLE_INHERIT_COND(CSSPropertyPaddingRight, paddingRight, PaddingRight)
            HANDLE_INHERIT_COND(CSSPropertyPaddingBottom, paddingBottom, PaddingBottom)
            HANDLE_INHERIT_COND(CSSPropertyPaddingLeft, paddingLeft, PaddingLeft)
            HANDLE_INHERIT_COND(CSSPropertyMarginTop, marginTop, MarginTop)
            HANDLE_INHERIT_COND(CSSPropertyMarginRight, marginRight, MarginRight)
            HANDLE_INHERIT_COND(CSSPropertyMarginBottom, marginBottom, MarginBottom)
            HANDLE_INHERIT_COND(CSSPropertyMarginLeft, marginLeft, MarginLeft)
            HANDLE_INHERIT_COND(CSSPropertyTextIndent, textIndent, TextIndent)
            return;
        }
        else if (isInitial) {
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyMaxWidth, MaxWidth, MaxSize)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyBottom, Bottom, Offset)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyTop, Top, Offset)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyLeft, Left, Offset)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyRight, Right, Offset)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyWidth, Width, Size)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyMinWidth, MinWidth, MinSize)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyPaddingTop, PaddingTop, Padding)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyPaddingRight, PaddingRight, Padding)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyPaddingBottom, PaddingBottom, Padding)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyPaddingLeft, PaddingLeft, Padding)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyMarginTop, MarginTop, Margin)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyMarginRight, MarginRight, Margin)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyMarginBottom, MarginBottom, Margin)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyMarginLeft, MarginLeft, Margin)
            HANDLE_INITIAL_COND(CSSPropertyTextIndent, TextIndent)
            return;
        } 

        if (primitiveValue && !apply) {
            int type = primitiveValue->primitiveType();
            if (type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG)
                // Handle our quirky margin units if we have them.
                l = Length(primitiveValue->computeLengthIntForLength(style(), zoomFactor), Fixed, 
                           primitiveValue->isQuirkValue());
            else if (type == CSSPrimitiveValue::CSS_PERCENTAGE)
                l = Length(primitiveValue->getDoubleValue(), Percent);
            else
                return;
            if (id == CSSPropertyPaddingLeft || id == CSSPropertyPaddingRight ||
                id == CSSPropertyPaddingTop || id == CSSPropertyPaddingBottom)
                // Padding can't be negative
                apply = !((l.isFixed() || l.isPercent()) && l.calcValue(100) < 0);
            else
                apply = true;
        }
        if (!apply) return;
        switch (id) {
            case CSSPropertyMaxWidth:
                m_style->setMaxWidth(l);
                break;
            case CSSPropertyBottom:
                m_style->setBottom(l);
                break;
            case CSSPropertyTop:
                m_style->setTop(l);
                break;
            case CSSPropertyLeft:
                m_style->setLeft(l);
                break;
            case CSSPropertyRight:
                m_style->setRight(l);
                break;
            case CSSPropertyWidth:
                m_style->setWidth(l);
                break;
            case CSSPropertyMinWidth:
                m_style->setMinWidth(l);
                break;
            case CSSPropertyPaddingTop:
                m_style->setPaddingTop(l);
                break;
            case CSSPropertyPaddingRight:
                m_style->setPaddingRight(l);
                break;
            case CSSPropertyPaddingBottom:
                m_style->setPaddingBottom(l);
                break;
            case CSSPropertyPaddingLeft:
                m_style->setPaddingLeft(l);
                break;
            case CSSPropertyMarginTop:
                m_style->setMarginTop(l);
                break;
            case CSSPropertyMarginRight:
                m_style->setMarginRight(l);
                break;
            case CSSPropertyMarginBottom:
                m_style->setMarginBottom(l);
                break;
            case CSSPropertyMarginLeft:
                m_style->setMarginLeft(l);
                break;
            case CSSPropertyTextIndent:
                m_style->setTextIndent(l);
                break;
            default:
                break;
            }
        return;
    }

    case CSSPropertyMaxHeight:
        if (primitiveValue && primitiveValue->getIdent() == CSSValueNone) {
            l = Length(undefinedLength, Fixed);
            apply = true;
        }
    case CSSPropertyHeight:
    case CSSPropertyMinHeight:
        if (primitiveValue && primitiveValue->getIdent() == CSSValueIntrinsic) {
            l = Length(Intrinsic);
            apply = true;
        } else if (primitiveValue && primitiveValue->getIdent() == CSSValueMinIntrinsic) {
            l = Length(MinIntrinsic);
            apply = true;
        } else if (id != CSSPropertyMaxHeight && primitiveValue && primitiveValue->getIdent() == CSSValueAuto)
            apply = true;
        if (isInherit) {
            HANDLE_INHERIT_COND(CSSPropertyMaxHeight, maxHeight, MaxHeight)
            HANDLE_INHERIT_COND(CSSPropertyHeight, height, Height)
            HANDLE_INHERIT_COND(CSSPropertyMinHeight, minHeight, MinHeight)
            return;
        }
        if (isInitial) {
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyMaxHeight, MaxHeight, MaxSize)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyHeight, Height, Size)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyMinHeight, MinHeight, MinSize)
            return;
        }

        if (primitiveValue && !apply) {
            unsigned short type = primitiveValue->primitiveType();
            if (type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG)
                l = Length(primitiveValue->computeLengthIntForLength(style(), zoomFactor), Fixed);
            else if (type == CSSPrimitiveValue::CSS_PERCENTAGE)
                l = Length(primitiveValue->getDoubleValue(), Percent);
            else
                return;
            apply = true;
        }
        if (apply)
            switch (id) {
                case CSSPropertyMaxHeight:
                    m_style->setMaxHeight(l);
                    break;
                case CSSPropertyHeight:
                    m_style->setHeight(l);
                    break;
                case CSSPropertyMinHeight:
                    m_style->setMinHeight(l);
                    break;
            }
        return;

    case CSSPropertyVerticalAlign:
        HANDLE_INHERIT_AND_INITIAL(verticalAlign, VerticalAlign)
        if (!primitiveValue)
            return;
        if (primitiveValue->getIdent()) {
          EVerticalAlign align;

          switch (primitiveValue->getIdent()) {
                case CSSValueTop:
                    align = TOP; break;
                case CSSValueBottom:
                    align = BOTTOM; break;
                case CSSValueMiddle:
                    align = MIDDLE; break;
                case CSSValueBaseline:
                    align = BASELINE; break;
                case CSSValueTextBottom:
                    align = TEXT_BOTTOM; break;
                case CSSValueTextTop:
                    align = TEXT_TOP; break;
                case CSSValueSub:
                    align = SUB; break;
                case CSSValueSuper:
                    align = SUPER; break;
                case CSSValueWebkitBaselineMiddle:
                    align = BASELINE_MIDDLE; break;
                default:
                    return;
            }
          m_style->setVerticalAlign(align);
          return;
        } else {
          int type = primitiveValue->primitiveType();
          Length l;
          if (type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG)
            l = Length(primitiveValue->computeLengthIntForLength(style(), zoomFactor), Fixed);
          else if (type == CSSPrimitiveValue::CSS_PERCENTAGE)
            l = Length(primitiveValue->getDoubleValue(), Percent);

          m_style->setVerticalAlign(LENGTH);
          m_style->setVerticalAlignLength(l);
        }
        return;

    case CSSPropertyFontSize:
    {
        FontDescription fontDescription = m_style->fontDescription();
        fontDescription.setKeywordSize(0);
        bool familyIsFixed = fontDescription.genericFamily() == FontDescription::MonospaceFamily;
        float oldSize = 0;
        float size = 0;
        
        bool parentIsAbsoluteSize = false;
        if (m_parentNode) {
            oldSize = m_parentStyle->fontDescription().specifiedSize();
            parentIsAbsoluteSize = m_parentStyle->fontDescription().isAbsoluteSize();
        }

        if (isInherit) {
            size = oldSize;
            if (m_parentNode)
                fontDescription.setKeywordSize(m_parentStyle->fontDescription().keywordSize());
        } else if (isInitial) {
            size = fontSizeForKeyword(CSSValueMedium, m_style->htmlHacks(), familyIsFixed);
            fontDescription.setKeywordSize(CSSValueMedium - CSSValueXxSmall + 1);
        } else if (primitiveValue->getIdent()) {
            // Keywords are being used.
            switch (primitiveValue->getIdent()) {
                case CSSValueXxSmall:
                case CSSValueXSmall:
                case CSSValueSmall:
                case CSSValueMedium:
                case CSSValueLarge:
                case CSSValueXLarge:
                case CSSValueXxLarge:
                case CSSValueWebkitXxxLarge:
                    size = fontSizeForKeyword(primitiveValue->getIdent(), m_style->htmlHacks(), familyIsFixed);
                    fontDescription.setKeywordSize(primitiveValue->getIdent() - CSSValueXxSmall + 1);
                    break;
                case CSSValueLarger:
                    size = largerFontSize(oldSize, m_style->htmlHacks());
                    break;
                case CSSValueSmaller:
                    size = smallerFontSize(oldSize, m_style->htmlHacks());
                    break;
                default:
                    return;
            }

            fontDescription.setIsAbsoluteSize(parentIsAbsoluteSize && 
                                              (primitiveValue->getIdent() == CSSValueLarger ||
                                               primitiveValue->getIdent() == CSSValueSmaller));
        } else {
            int type = primitiveValue->primitiveType();
            fontDescription.setIsAbsoluteSize(parentIsAbsoluteSize ||
                                              (type != CSSPrimitiveValue::CSS_PERCENTAGE &&
                                               type != CSSPrimitiveValue::CSS_EMS && 
                                               type != CSSPrimitiveValue::CSS_EXS));
            if (type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG)
                size = primitiveValue->computeLengthFloat(m_parentStyle, true);
            else if (type == CSSPrimitiveValue::CSS_PERCENTAGE)
                size = (primitiveValue->getFloatValue() * oldSize) / 100.0f;
            else
                return;
        }

        if (size < 0)
            return;

        setFontSize(fontDescription, size);
        if (m_style->setFontDescription(fontDescription))
            m_fontDirty = true;
        return;
    }

    case CSSPropertyZIndex: {
        if (isInherit) {
            if (m_parentStyle->hasAutoZIndex())
                m_style->setHasAutoZIndex();
            else
                m_style->setZIndex(m_parentStyle->zIndex());
            return;
        } else if (isInitial || primitiveValue->getIdent() == CSSValueAuto) {
            m_style->setHasAutoZIndex();
            return;
        }
        
        // FIXME: Should clamp all sorts of other integer properties too.
        const double minIntAsDouble = INT_MIN;
        const double maxIntAsDouble = INT_MAX;
        m_style->setZIndex(static_cast<int>(max(minIntAsDouble, min(primitiveValue->getDoubleValue(), maxIntAsDouble))));
        return;
    }
    case CSSPropertyWidows:
    {
        HANDLE_INHERIT_AND_INITIAL(widows, Widows)
        if (!primitiveValue || primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_NUMBER)
            return;
        m_style->setWidows(primitiveValue->getIntValue());
        return;
    }
        
    case CSSPropertyOrphans:
    {
        HANDLE_INHERIT_AND_INITIAL(orphans, Orphans)
        if (!primitiveValue || primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_NUMBER)
            return;
        m_style->setOrphans(primitiveValue->getIntValue());
        return;
    }        

// length, percent, number
    case CSSPropertyLineHeight:
    {
        HANDLE_INHERIT_AND_INITIAL(lineHeight, LineHeight)
        if (!primitiveValue)
            return;
        Length lineHeight;
        int type = primitiveValue->primitiveType();
        if (primitiveValue->getIdent() == CSSValueNormal)
            lineHeight = Length(-100.0, Percent);
        else if (type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG) {
            double multiplier = m_style->effectiveZoom();
            if (m_style->textSizeAdjust() && m_checker.m_document->frame() && m_checker.m_document->frame()->shouldApplyTextZoom())
                multiplier *= m_checker.m_document->frame()->textZoomFactor();
            lineHeight = Length(primitiveValue->computeLengthIntForLength(style(), multiplier), Fixed);
        } else if (type == CSSPrimitiveValue::CSS_PERCENTAGE)
            lineHeight = Length((m_style->fontSize() * primitiveValue->getIntValue()) / 100, Fixed);
        else if (type == CSSPrimitiveValue::CSS_NUMBER)
            lineHeight = Length(primitiveValue->getDoubleValue() * 100.0, Percent);
        else
            return;
        m_style->setLineHeight(lineHeight);
        return;
    }

// string
    case CSSPropertyTextAlign:
    {
        HANDLE_INHERIT_AND_INITIAL(textAlign, TextAlign)
        if (!primitiveValue)
            return;
        int id = primitiveValue->getIdent();
        if (id == CSSValueStart)
            m_style->setTextAlign(m_style->direction() == LTR ? LEFT : RIGHT);
        else if (id == CSSValueEnd)
            m_style->setTextAlign(m_style->direction() == LTR ? RIGHT : LEFT);
        else
            m_style->setTextAlign(*primitiveValue);
        return;
    }

// rect
    case CSSPropertyClip:
    {
        Length top;
        Length right;
        Length bottom;
        Length left;
        bool hasClip = true;
        if (isInherit) {
            if (m_parentStyle->hasClip()) {
                top = m_parentStyle->clipTop();
                right = m_parentStyle->clipRight();
                bottom = m_parentStyle->clipBottom();
                left = m_parentStyle->clipLeft();
            }
            else {
                hasClip = false;
                top = right = bottom = left = Length();
            }
        } else if (isInitial) {
            hasClip = false;
            top = right = bottom = left = Length();
        } else if (!primitiveValue) {
            return;
        } else if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_RECT) {
            Rect* rect = primitiveValue->getRectValue();
            if (!rect)
                return;
            top = convertToLength(rect->top(), style());
            right = convertToLength(rect->right(), style());
            bottom = convertToLength(rect->bottom(), style());
            left = convertToLength(rect->left(), style());

        } else if (primitiveValue->getIdent() != CSSValueAuto) {
            return;
        }
        m_style->setClip(top, right, bottom, left);
        m_style->setHasClip(hasClip);
   
        // rect, ident
        return;
    }

// lists
    case CSSPropertyContent:
        // list of string, uri, counter, attr, i
    {
        // FIXME: In CSS3, it will be possible to inherit content.  In CSS2 it is not.  This
        // note is a reminder that eventually "inherit" needs to be supported.

        if (isInitial) {
            m_style->clearContent();
            return;
        }
       
        if (!value->isValueList())
            return;

        CSSValueList* list = static_cast<CSSValueList*>(value);
        int len = list->length();

        bool didSet = false;
        for (int i = 0; i < len; i++) {
            CSSValue* item = list->itemWithoutBoundsCheck(i);
            if (item->isImageGeneratorValue()) {
                m_style->setContent(static_cast<CSSImageGeneratorValue*>(item)->generatedImage(), didSet);
                didSet = true;
            }
           
            if (!item->isPrimitiveValue())
                continue;
           
            CSSPrimitiveValue* val = static_cast<CSSPrimitiveValue*>(item);
            switch (val->primitiveType()) {
                case CSSPrimitiveValue::CSS_STRING:
                    m_style->setContent(val->getStringValue().impl(), didSet);
                    didSet = true;
                    break;
                case CSSPrimitiveValue::CSS_ATTR: {
                    // FIXME: Can a namespace be specified for an attr(foo)?
                    if (m_style->styleType() == RenderStyle::NOPSEUDO)
                        m_style->setUnique();
                    else
                        m_parentStyle->setUnique();
                    QualifiedName attr(nullAtom, val->getStringValue().impl(), nullAtom);
                    m_style->setContent(m_element->getAttribute(attr).impl(), didSet);
                    didSet = true;
                    // register the fact that the attribute value affects the style
                    m_selectorAttrs.add(attr.localName().impl());
                    break;
                }
                case CSSPrimitiveValue::CSS_URI: {
                    CSSImageValue* image = static_cast<CSSImageValue*>(val);
                    m_style->setContent(image->cachedImage(m_element->document()->docLoader()), didSet);
                    didSet = true;
                    break;
                }
                case CSSPrimitiveValue::CSS_COUNTER: {
                    Counter* counterValue = val->getCounterValue();
                    CounterContent* counter = new CounterContent(counterValue->identifier(),
                        (EListStyleType)counterValue->listStyleNumber(), counterValue->separator());
                    m_style->setContent(counter, didSet);
                    didSet = true;
                }
            }
        }
        if (!didSet)
            m_style->clearContent();
        return;
    }

    case CSSPropertyCounterIncrement:
        applyCounterList(style(), value->isValueList() ? static_cast<CSSValueList*>(value) : 0, false);
        return;
    case CSSPropertyCounterReset:
        applyCounterList(style(), value->isValueList() ? static_cast<CSSValueList*>(value) : 0, true);
        return;

    case CSSPropertyFontFamily: {
        // list of strings and ids
        if (isInherit) {
            FontDescription parentFontDescription = m_parentStyle->fontDescription();
            FontDescription fontDescription = m_style->fontDescription();
            fontDescription.setGenericFamily(parentFontDescription.genericFamily());
            fontDescription.setFamily(parentFontDescription.firstFamily());
            if (m_style->setFontDescription(fontDescription))
                m_fontDirty = true;
            return;
        }
        else if (isInitial) {
            FontDescription initialDesc = FontDescription();
            FontDescription fontDescription = m_style->fontDescription();
            // We need to adjust the size to account for the generic family change from monospace
            // to non-monospace.
            if (fontDescription.keywordSize() && fontDescription.genericFamily() == FontDescription::MonospaceFamily)
                setFontSize(fontDescription, fontSizeForKeyword(CSSValueXxSmall + fontDescription.keywordSize() - 1, m_style->htmlHacks(), false));
            fontDescription.setGenericFamily(initialDesc.genericFamily());
            if (!initialDesc.firstFamily().familyIsEmpty())
                fontDescription.setFamily(initialDesc.firstFamily());
            if (m_style->setFontDescription(fontDescription))
                m_fontDirty = true;
            return;
        }
       
        if (!value->isValueList()) return;
        FontDescription fontDescription = m_style->fontDescription();
        CSSValueList *list = static_cast<CSSValueList*>(value);
        int len = list->length();
        FontFamily& firstFamily = fontDescription.firstFamily();
        FontFamily *currFamily = 0;
       
        // Before mapping in a new font-family property, we should reset the generic family.
        bool oldFamilyIsMonospace = fontDescription.genericFamily() == FontDescription::MonospaceFamily;
        fontDescription.setGenericFamily(FontDescription::NoFamily);

        for (int i = 0; i < len; i++) {
            CSSValue *item = list->itemWithoutBoundsCheck(i);
            if (!item->isPrimitiveValue()) continue;
            CSSPrimitiveValue *val = static_cast<CSSPrimitiveValue*>(item);
            AtomicString face;
            Settings* settings = m_checker.m_document->settings();
            if (val->primitiveType() == CSSPrimitiveValue::CSS_STRING)
                face = static_cast<FontFamilyValue*>(val)->familyName();
            else if (val->primitiveType() == CSSPrimitiveValue::CSS_IDENT && settings) {
                switch (val->getIdent()) {
                    case CSSValueWebkitBody:
                        face = settings->standardFontFamily();
                        break;
                    case CSSValueSerif:
                        face = "-webkit-serif";
                        fontDescription.setGenericFamily(FontDescription::SerifFamily);
                        break;
                    case CSSValueSansSerif:
                        face = "-webkit-sans-serif";
                        fontDescription.setGenericFamily(FontDescription::SansSerifFamily);
                        break;
                    case CSSValueCursive:
                        face = "-webkit-cursive";
                        fontDescription.setGenericFamily(FontDescription::CursiveFamily);
                        break;
                    case CSSValueFantasy:
                        face = "-webkit-fantasy";
                        fontDescription.setGenericFamily(FontDescription::FantasyFamily);
                        break;
                    case CSSValueMonospace:
                        face = "-webkit-monospace";
                        fontDescription.setGenericFamily(FontDescription::MonospaceFamily);
                        break;
                }
            }
   
            if (!face.isEmpty()) {
                if (!currFamily) {
                    // Filling in the first family.
                    firstFamily.setFamily(face);
                    currFamily = &firstFamily;
                }
                else {
                    RefPtr<SharedFontFamily> newFamily = SharedFontFamily::create();
                    newFamily->setFamily(face);
                    currFamily->appendFamily(newFamily);
                    currFamily = newFamily.get();
                }
   
                if (fontDescription.keywordSize() && (fontDescription.genericFamily() == FontDescription::MonospaceFamily) != oldFamilyIsMonospace)
                    setFontSize(fontDescription, fontSizeForKeyword(CSSValueXxSmall + fontDescription.keywordSize() - 1, m_style->htmlHacks(), !oldFamilyIsMonospace));
           
                if (m_style->setFontDescription(fontDescription))
                    m_fontDirty = true;
            }
        }
      return;
    }
    case CSSPropertyTextDecoration: {
        // list of ident
        HANDLE_INHERIT_AND_INITIAL(textDecoration, TextDecoration)
        int t = RenderStyle::initialTextDecoration();
        if (primitiveValue && primitiveValue->getIdent() == CSSValueNone) {
            // do nothing
        } else {
            if (!value->isValueList()) return;
            CSSValueList *list = static_cast<CSSValueList*>(value);
            int len = list->length();
            for (int i = 0; i < len; i++)
            {
                CSSValue *item = list->itemWithoutBoundsCheck(i);
                if (!item->isPrimitiveValue()) continue;
                primitiveValue = static_cast<CSSPrimitiveValue*>(item);
                switch (primitiveValue->getIdent()) {
                    case CSSValueNone:
                        t = TDNONE; break;
                    case CSSValueUnderline:
                        t |= UNDERLINE; break;
                    case CSSValueOverline:
                        t |= OVERLINE; break;
                    case CSSValueLineThrough:
                        t |= LINE_THROUGH; break;
                    case CSSValueBlink:
                        t |= BLINK; break;
                    default:
                        return;
                }
            }
        }

        m_style->setTextDecoration(t);
        return;
    }

    case CSSPropertyZoom:
    {
        // Reset the zoom in effect before we do anything.  This allows the setZoom method to accurately compute a new
        // zoom in effect.
        m_style->setEffectiveZoom(m_parentStyle ? m_parentStyle->effectiveZoom() : RenderStyle::initialZoom());
       
        // Now we can handle inherit and initial.
        HANDLE_INHERIT_AND_INITIAL(zoom, Zoom)
       
        // Handle normal/reset, numbers and percentages.
        int type = primitiveValue->primitiveType();
        if (primitiveValue->getIdent() == CSSValueNormal)
            m_style->setZoom(RenderStyle::initialZoom());
        else if (primitiveValue->getIdent() == CSSValueReset) {
            m_style->setEffectiveZoom(RenderStyle::initialZoom());
            m_style->setZoom(RenderStyle::initialZoom());
        } else if (type == CSSPrimitiveValue::CSS_PERCENTAGE) {
            if (primitiveValue->getFloatValue())
                m_style->setZoom(primitiveValue->getFloatValue() / 100.0f);
        } else if (type == CSSPrimitiveValue::CSS_NUMBER) {
            if (primitiveValue->getFloatValue())
                m_style->setZoom(primitiveValue->getFloatValue());
        }
       
        m_fontDirty = true;
        return;
    }
// shorthand properties
    case CSSPropertyBackground:
        if (isInitial) {
            m_style->clearBackgroundLayers();
            m_style->setBackgroundColor(Color());
        }
        else if (isInherit) {
            m_style->inheritBackgroundLayers(*m_parentStyle->backgroundLayers());
            m_style->setBackgroundColor(m_parentStyle->backgroundColor());
        }
        return;
    case CSSPropertyWebkitMask:
        if (isInitial)
            m_style->clearMaskLayers();
        else if (isInherit)
            m_style->inheritMaskLayers(*m_parentStyle->maskLayers());
        return;

    case CSSPropertyBorder:
    case CSSPropertyBorderStyle:
    case CSSPropertyBorderWidth:
    case CSSPropertyBorderColor:
        if (id == CSSPropertyBorder || id == CSSPropertyBorderColor)
        {
            if (isInherit) {
                m_style->setBorderTopColor(m_parentStyle->borderTopColor().isValid() ? m_parentStyle->borderTopColor() : m_parentStyle->color());
                m_style->setBorderBottomColor(m_parentStyle->borderBottomColor().isValid() ? m_parentStyle->borderBottomColor() : m_parentStyle->color());
                m_style->setBorderLeftColor(m_parentStyle->borderLeftColor().isValid() ? m_parentStyle->borderLeftColor() : m_parentStyle->color());
                m_style->setBorderRightColor(m_parentStyle->borderRightColor().isValid() ? m_parentStyle->borderRightColor(): m_parentStyle->color());
            }
            else if (isInitial) {
                m_style->setBorderTopColor(Color()); // Reset to invalid color so currentColor is used instead.
                m_style->setBorderBottomColor(Color());
                m_style->setBorderLeftColor(Color());
                m_style->setBorderRightColor(Color());
            }
        }
        if (id == CSSPropertyBorder || id == CSSPropertyBorderStyle)
        {
            if (isInherit) {
                m_style->setBorderTopStyle(m_parentStyle->borderTopStyle());
                m_style->setBorderBottomStyle(m_parentStyle->borderBottomStyle());
                m_style->setBorderLeftStyle(m_parentStyle->borderLeftStyle());
                m_style->setBorderRightStyle(m_parentStyle->borderRightStyle());
            }
            else if (isInitial) {
                m_style->setBorderTopStyle(RenderStyle::initialBorderStyle());
                m_style->setBorderBottomStyle(RenderStyle::initialBorderStyle());
                m_style->setBorderLeftStyle(RenderStyle::initialBorderStyle());
                m_style->setBorderRightStyle(RenderStyle::initialBorderStyle());
            }
        }
        if (id == CSSPropertyBorder || id == CSSPropertyBorderWidth)
        {
            if (isInherit) {
                m_style->setBorderTopWidth(m_parentStyle->borderTopWidth());
                m_style->setBorderBottomWidth(m_parentStyle->borderBottomWidth());
                m_style->setBorderLeftWidth(m_parentStyle->borderLeftWidth());
                m_style->setBorderRightWidth(m_parentStyle->borderRightWidth());
            }
            else if (isInitial) {
                m_style->setBorderTopWidth(RenderStyle::initialBorderWidth());
                m_style->setBorderBottomWidth(RenderStyle::initialBorderWidth());
                m_style->setBorderLeftWidth(RenderStyle::initialBorderWidth());
                m_style->setBorderRightWidth(RenderStyle::initialBorderWidth());
            }
        }
        return;
    case CSSPropertyBorderTop:
        if (isInherit) {
            m_style->setBorderTopColor(m_parentStyle->borderTopColor().isValid() ? m_parentStyle->borderTopColor() : m_parentStyle->color());
            m_style->setBorderTopStyle(m_parentStyle->borderTopStyle());
            m_style->setBorderTopWidth(m_parentStyle->borderTopWidth());
        }
        else if (isInitial)
            m_style->resetBorderTop();
        return;
    case CSSPropertyBorderRight:
        if (isInherit) {
            m_style->setBorderRightColor(m_parentStyle->borderRightColor().isValid() ? m_parentStyle->borderRightColor() : m_parentStyle->color());
            m_style->setBorderRightStyle(m_parentStyle->borderRightStyle());
            m_style->setBorderRightWidth(m_parentStyle->borderRightWidth());
        }
        else if (isInitial)
            m_style->resetBorderRight();
        return;
    case CSSPropertyBorderBottom:
        if (isInherit) {
            m_style->setBorderBottomColor(m_parentStyle->borderBottomColor().isValid() ? m_parentStyle->borderBottomColor() : m_parentStyle->color());
            m_style->setBorderBottomStyle(m_parentStyle->borderBottomStyle());
            m_style->setBorderBottomWidth(m_parentStyle->borderBottomWidth());
        }
        else if (isInitial)
            m_style->resetBorderBottom();
        return;
    case CSSPropertyBorderLeft:
        if (isInherit) {
            m_style->setBorderLeftColor(m_parentStyle->borderLeftColor().isValid() ? m_parentStyle->borderLeftColor() : m_parentStyle->color());
            m_style->setBorderLeftStyle(m_parentStyle->borderLeftStyle());
            m_style->setBorderLeftWidth(m_parentStyle->borderLeftWidth());
        }
        else if (isInitial)
            m_style->resetBorderLeft();
        return;
    case CSSPropertyMargin:
        if (isInherit) {
            m_style->setMarginTop(m_parentStyle->marginTop());
            m_style->setMarginBottom(m_parentStyle->marginBottom());
            m_style->setMarginLeft(m_parentStyle->marginLeft());
            m_style->setMarginRight(m_parentStyle->marginRight());
        }
        else if (isInitial)
            m_style->resetMargin();
        return;
    case CSSPropertyPadding:
        if (isInherit) {
            m_style->setPaddingTop(m_parentStyle->paddingTop());
            m_style->setPaddingBottom(m_parentStyle->paddingBottom());
            m_style->setPaddingLeft(m_parentStyle->paddingLeft());
            m_style->setPaddingRight(m_parentStyle->paddingRight());
        }
        else if (isInitial)
            m_style->resetPadding();
        return;
    case CSSPropertyFont:
        if (isInherit) {
            FontDescription fontDescription = m_parentStyle->fontDescription();
            m_style->setLineHeight(m_parentStyle->lineHeight());
            m_lineHeightValue = 0;
            if (m_style->setFontDescription(fontDescription))
                m_fontDirty = true;
        } else if (isInitial) {
            Settings* settings = m_checker.m_document->settings();
            FontDescription fontDescription;
            fontDescription.setGenericFamily(FontDescription::StandardFamily);
            fontDescription.setRenderingMode(settings->fontRenderingMode());
            fontDescription.setUsePrinterFont(m_checker.m_document->printing());
            const AtomicString& standardFontFamily = m_checker.m_document->settings()->standardFontFamily();
            if (!standardFontFamily.isEmpty()) {
                fontDescription.firstFamily().setFamily(standardFontFamily);
                fontDescription.firstFamily().appendFamily(0);
            }
            fontDescription.setKeywordSize(CSSValueMedium - CSSValueXxSmall + 1);
            setFontSize(fontDescription, fontSizeForKeyword(CSSValueMedium, m_style->htmlHacks(), false));
            m_style->setLineHeight(RenderStyle::initialLineHeight());
            m_lineHeightValue = 0;
            if (m_style->setFontDescription(fontDescription))
                m_fontDirty = true;
        } else if (primitiveValue) {
            m_style->setLineHeight(RenderStyle::initialLineHeight());
            m_lineHeightValue = 0;
            FontDescription fontDescription;
            theme()->systemFont(primitiveValue->getIdent(), fontDescription);
            // Double-check and see if the theme did anything.  If not, don't bother updating the font.
            if (fontDescription.isAbsoluteSize()) {
                // Handle the zoom factor.
                fontDescription.setComputedSize(getComputedSizeFromSpecifiedSize(fontDescription.isAbsoluteSize(), fontDescription.specifiedSize()));
                if (m_style->setFontDescription(fontDescription))
                    m_fontDirty = true;
            }
        } else if (value->isFontValue()) {
            FontValue *font = static_cast<FontValue*>(value);
            if (!font->style || !font->variant || !font->weight ||
                 !font->size || !font->lineHeight || !font->family)
                return;
            applyProperty(CSSPropertyFontStyle, font->style.get());
            applyProperty(CSSPropertyFontVariant, font->variant.get());
            applyProperty(CSSPropertyFontWeight, font->weight.get());
            applyProperty(CSSPropertyFontSize, font->size.get());

            m_lineHeightValue = font->lineHeight.get();

            applyProperty(CSSPropertyFontFamily, font->family.get());
        }
        return;
       
    case CSSPropertyListStyle:
        if (isInherit) {
            m_style->setListStyleType(m_parentStyle->listStyleType());
            m_style->setListStyleImage(m_parentStyle->listStyleImage());
            m_style->setListStylePosition(m_parentStyle->listStylePosition());
        }
        else if (isInitial) {
            m_style->setListStyleType(RenderStyle::initialListStyleType());
            m_style->setListStyleImage(RenderStyle::initialListStyleImage());
            m_style->setListStylePosition(RenderStyle::initialListStylePosition());
        }
        return;
    case CSSPropertyOutline:
        if (isInherit) {
            m_style->setOutlineWidth(m_parentStyle->outlineWidth());
            m_style->setOutlineColor(m_parentStyle->outlineColor().isValid() ? m_parentStyle->outlineColor() : m_parentStyle->color());
            m_style->setOutlineStyle(m_parentStyle->outlineStyle());
        }
        else if (isInitial)
            m_style->resetOutline();
        return;

    // CSS3 Properties
    case CSSPropertyWebkitAppearance: {
        HANDLE_INHERIT_AND_INITIAL(appearance, Appearance)
        if (!primitiveValue)
            return;
        m_style->setAppearance(*primitiveValue);
        return;
    }
    case CSSPropertyWebkitBinding: {
#if ENABLE(XBL)
        if (isInitial || (primitiveValue && primitiveValue->getIdent() == CSSValueNone)) {
            m_style->deleteBindingURIs();
            return;
        }
        else if (isInherit) {
            if (m_parentStyle->bindingURIs())
                m_style->inheritBindingURIs(m_parentStyle->bindingURIs());
            else
                m_style->deleteBindingURIs();
            return;
        }

        if (!value->isValueList()) return;
        CSSValueList* list = static_cast<CSSValueList*>(value);
        bool firstBinding = true;
        for (unsigned int i = 0; i < list->length(); i++) {
            CSSValue *item = list->itemWithoutBoundsCheck(i);
            CSSPrimitiveValue *val = static_cast<CSSPrimitiveValue*>(item);
            if (val->primitiveType() == CSSPrimitiveValue::CSS_URI) {
                if (firstBinding) {
                    firstBinding = false;
                    m_style->deleteBindingURIs();
                }
                m_style->addBindingURI(val->getStringValue());
            }
        }
#endif
        return;
    }

    case CSSPropertyWebkitBorderImage:
    case CSSPropertyWebkitMaskBoxImage: {
        if (isInherit) {
            HANDLE_INHERIT_COND(CSSPropertyWebkitBorderImage, borderImage, BorderImage)
            HANDLE_INHERIT_COND(CSSPropertyWebkitMaskBoxImage, maskBoxImage, MaskBoxImage)
            return;
        } else if (isInitial) {
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyWebkitBorderImage, BorderImage, NinePieceImage)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyWebkitMaskBoxImage, MaskBoxImage, NinePieceImage)
            return;
        }

        NinePieceImage image;
        mapNinePieceImage(value, image);
       
        if (id == CSSPropertyWebkitBorderImage)
            m_style->setBorderImage(image);
        else
            m_style->setMaskBoxImage(image);
        return;
    }

    case CSSPropertyWebkitBorderRadius:
        if (isInherit) {
            m_style->setBorderTopLeftRadius(m_parentStyle->borderTopLeftRadius());
            m_style->setBorderTopRightRadius(m_parentStyle->borderTopRightRadius());
            m_style->setBorderBottomLeftRadius(m_parentStyle->borderBottomLeftRadius());
            m_style->setBorderBottomRightRadius(m_parentStyle->borderBottomRightRadius());
            return;
        }
        if (isInitial) {
            m_style->resetBorderRadius();
            return;
        }
        // Fall through
    case CSSPropertyWebkitBorderTopLeftRadius:
    case CSSPropertyWebkitBorderTopRightRadius:
    case CSSPropertyWebkitBorderBottomLeftRadius:
    case CSSPropertyWebkitBorderBottomRightRadius: {
        if (isInherit) {
            HANDLE_INHERIT_COND(CSSPropertyWebkitBorderTopLeftRadius, borderTopLeftRadius, BorderTopLeftRadius)
            HANDLE_INHERIT_COND(CSSPropertyWebkitBorderTopRightRadius, borderTopRightRadius, BorderTopRightRadius)
            HANDLE_INHERIT_COND(CSSPropertyWebkitBorderBottomLeftRadius, borderBottomLeftRadius, BorderBottomLeftRadius)
            HANDLE_INHERIT_COND(CSSPropertyWebkitBorderBottomRightRadius, borderBottomRightRadius, BorderBottomRightRadius)
            return;
        }
       
        if (isInitial) {
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyWebkitBorderTopLeftRadius, BorderTopLeftRadius, BorderRadius)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyWebkitBorderTopRightRadius, BorderTopRightRadius, BorderRadius)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyWebkitBorderBottomLeftRadius, BorderBottomLeftRadius, BorderRadius)
            HANDLE_INITIAL_COND_WITH_VALUE(CSSPropertyWebkitBorderBottomRightRadius, BorderBottomRightRadius, BorderRadius)
            return;
        }

        if (!primitiveValue)
            return;

        Pair* pair = primitiveValue->getPairValue();
        if (!pair)
            return;

        int width = pair->first()->computeLengthInt(style(), zoomFactor);
        int height = pair->second()->computeLengthInt(style(), zoomFactor);
        if (width < 0 || height < 0)
            return;

        if (width == 0)
            height = 0; // Null out the other value.
        else if (height == 0)
            width = 0; // Null out the other value.

        IntSize size(width, height);
        switch (id) {
            case CSSPropertyWebkitBorderTopLeftRadius:
                m_style->setBorderTopLeftRadius(size);
                break;
            case CSSPropertyWebkitBorderTopRightRadius:
                m_style->setBorderTopRightRadius(size);
                break;
            case CSSPropertyWebkitBorderBottomLeftRadius:
                m_style->setBorderBottomLeftRadius(size);
                break;
            case CSSPropertyWebkitBorderBottomRightRadius:
                m_style->setBorderBottomRightRadius(size);
                break;
            default:
                m_style->setBorderRadius(size);
                break;
        }
        return;
    }

    case CSSPropertyOutlineOffset:
        HANDLE_INHERIT_AND_INITIAL(outlineOffset, OutlineOffset)
        m_style->setOutlineOffset(primitiveValue->computeLengthInt(style(), zoomFactor));
        return;

    case CSSPropertyTextShadow:
    case CSSPropertyWebkitBoxShadow: {
        if (isInherit) {
            if (id == CSSPropertyTextShadow)
                return m_style->setTextShadow(m_parentStyle->textShadow() ? new ShadowData(*m_parentStyle->textShadow()) : 0);
            return m_style->setBoxShadow(m_parentStyle->boxShadow() ? new ShadowData(*m_parentStyle->boxShadow()) : 0);
        }
        if (isInitial || primitiveValue) // initial | none
            return id == CSSPropertyTextShadow ? m_style->setTextShadow(0) : m_style->setBoxShadow(0);

        if (!value->isValueList())
            return;

        CSSValueList *list = static_cast<CSSValueList*>(value);
        int len = list->length();
        for (int i = 0; i < len; i++) {
            ShadowValue* item = static_cast<ShadowValue*>(list->itemWithoutBoundsCheck(i));
            int x = item->x->computeLengthInt(style(), zoomFactor);
            int y = item->y->computeLengthInt(style(), zoomFactor);
            int blur = item->blur ? item->blur->computeLengthInt(style(), zoomFactor) : 0;
            Color color;
            if (item->color)
                color = getColorFromPrimitiveValue(item->color.get());
            ShadowData* shadowData = new ShadowData(x, y, blur, color.isValid() ? color : Color::transparent);
            if (id == CSSPropertyTextShadow)
                m_style->setTextShadow(shadowData, i != 0);
            else
                m_style->setBoxShadow(shadowData, i != 0);
        }
        return;
    }
    case CSSPropertyWebkitBoxReflect: {
        HANDLE_INHERIT_AND_INITIAL(boxReflect, BoxReflect)
        if (primitiveValue) {
            m_style->setBoxReflect(RenderStyle::initialBoxReflect());
            return;
        }
        CSSReflectValue* reflectValue = static_cast<CSSReflectValue*>(value);
        RefPtr<StyleReflection> reflection = StyleReflection::create();
        reflection->setDirection(reflectValue->direction());
        if (reflectValue->offset()) {
            int type = reflectValue->offset()->primitiveType();
            if (type == CSSPrimitiveValue::CSS_PERCENTAGE)
                reflection->setOffset(Length(reflectValue->offset()->getDoubleValue(), Percent));
            else
                reflection->setOffset(Length(reflectValue->offset()->computeLengthIntForLength(style(), zoomFactor), Fixed));
        }
        NinePieceImage mask;
        mapNinePieceImage(reflectValue->mask(), mask);
        reflection->setMask(mask);
       
        m_style->setBoxReflect(reflection.release());
        return;
    }
    case CSSPropertyOpacity:
        HANDLE_INHERIT_AND_INITIAL(opacity, Opacity)
        if (!primitiveValue || primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_NUMBER)
            return; // Error case.
        // Clamp opacity to the range 0-1
        m_style->setOpacity(min(1.0f, max(0.0f, primitiveValue->getFloatValue())));
        return;
    case CSSPropertyWebkitBoxAlign:
    {
        HANDLE_INHERIT_AND_INITIAL(boxAlign, BoxAlign)
        if (!primitiveValue)
            return;
        EBoxAlignment boxAlignment = *primitiveValue;
        if (boxAlignment != BJUSTIFY)
            m_style->setBoxAlign(boxAlignment);
        return;
    }
    case CSSPropertySrc: // Only used in @font-face rules.
        return;
    case CSSPropertyUnicodeRange: // Only used in @font-face rules.
        return;
    case CSSPropertyWebkitBoxDirection:
        HANDLE_INHERIT_AND_INITIAL(boxDirection, BoxDirection)
        if (primitiveValue)
            m_style->setBoxDirection(*primitiveValue);
        return;       
    case CSSPropertyWebkitBoxLines:
        HANDLE_INHERIT_AND_INITIAL(boxLines, BoxLines)
        if (primitiveValue)
            m_style->setBoxLines(*primitiveValue);
        return;     
    case CSSPropertyWebkitBoxOrient:
        HANDLE_INHERIT_AND_INITIAL(boxOrient, BoxOrient)
        if (primitiveValue)
            m_style->setBoxOrient(*primitiveValue);
        return;     
    case CSSPropertyWebkitBoxPack:
    {
        HANDLE_INHERIT_AND_INITIAL(boxPack, BoxPack)
        if (!primitiveValue)
            return;
        EBoxAlignment boxPack = *primitiveValue;
        if (boxPack != BSTRETCH && boxPack != BBASELINE)
            m_style->setBoxPack(boxPack);
        return;
    }
    case CSSPropertyWebkitBoxFlex:
        HANDLE_INHERIT_AND_INITIAL(boxFlex, BoxFlex)
        if (!primitiveValue || primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_NUMBER)
            return; // Error case.
        m_style->setBoxFlex(primitiveValue->getFloatValue());
        return;
    case CSSPropertyWebkitBoxFlexGroup:
        HANDLE_INHERIT_AND_INITIAL(boxFlexGroup, BoxFlexGroup)
        if (!primitiveValue || primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_NUMBER)
            return; // Error case.
        m_style->setBoxFlexGroup((unsigned int)(primitiveValue->getDoubleValue()));
        return;       
    case CSSPropertyWebkitBoxOrdinalGroup:
        HANDLE_INHERIT_AND_INITIAL(boxOrdinalGroup, BoxOrdinalGroup)
        if (!primitiveValue || primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_NUMBER)
            return; // Error case.
        m_style->setBoxOrdinalGroup((unsigned int)(primitiveValue->getDoubleValue()));
        return;
    case CSSPropertyWebkitBoxSizing:
        HANDLE_INHERIT_AND_INITIAL(boxSizing, BoxSizing)
        if (!primitiveValue)
            return;
        if (primitiveValue->getIdent() == CSSValueContentBox)
            m_style->setBoxSizing(CONTENT_BOX);
        else
            m_style->setBoxSizing(BORDER_BOX);
        return;
    case CSSPropertyWebkitColumnCount: {
        if (isInherit) {
            if (m_parentStyle->hasAutoColumnCount())
                m_style->setHasAutoColumnCount();
            else
                m_style->setColumnCount(m_parentStyle->columnCount());
            return;
        } else if (isInitial || primitiveValue->getIdent() == CSSValueAuto) {
            m_style->setHasAutoColumnCount();
            return;
        }
        m_style->setColumnCount(static_cast<unsigned short>(primitiveValue->getDoubleValue()));
        return;
    }
    case CSSPropertyWebkitColumnGap: {
        if (isInherit) {
            if (m_parentStyle->hasNormalColumnGap())
                m_style->setHasNormalColumnGap();
            else
                m_style->setColumnGap(m_parentStyle->columnGap());
            return;
        } else if (isInitial || primitiveValue->getIdent() == CSSValueNormal) {
            m_style->setHasNormalColumnGap();
            return;
        }
        m_style->setColumnGap(primitiveValue->computeLengthFloat(style(), zoomFactor));
        return;
    }
    case CSSPropertyWebkitColumnWidth: {
        if (isInherit) {
            if (m_parentStyle->hasAutoColumnWidth())
                m_style->setHasAutoColumnWidth();
            else
                m_style->setColumnWidth(m_parentStyle->columnWidth());
            return;
        } else if (isInitial || primitiveValue->getIdent() == CSSValueAuto) {
            m_style->setHasAutoColumnWidth();
            return;
        }
        m_style->setColumnWidth(primitiveValue->computeLengthFloat(style(), zoomFactor));
        return;
    }
    case CSSPropertyWebkitColumnRuleStyle:
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(columnRuleStyle, ColumnRuleStyle, BorderStyle)
        m_style->setColumnRuleStyle(*primitiveValue);
        return;
    case CSSPropertyWebkitColumnBreakBefore: {
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(columnBreakBefore, ColumnBreakBefore, PageBreak)
        m_style->setColumnBreakBefore(*primitiveValue);
        return;
    }
    case CSSPropertyWebkitColumnBreakAfter: {
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(columnBreakAfter, ColumnBreakAfter, PageBreak)
        m_style->setColumnBreakAfter(*primitiveValue);
        return;
    }
    case CSSPropertyWebkitColumnBreakInside: {
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(columnBreakInside, ColumnBreakInside, PageBreak)
        EPageBreak pb = *primitiveValue;
        if (pb != PBALWAYS)
            m_style->setColumnBreakInside(pb);
        return;
    }
     case CSSPropertyWebkitColumnRule:
        if (isInherit) {
            m_style->setColumnRuleColor(m_parentStyle->columnRuleColor().isValid() ? m_parentStyle->columnRuleColor() : m_parentStyle->color());
            m_style->setColumnRuleStyle(m_parentStyle->columnRuleStyle());
            m_style->setColumnRuleWidth(m_parentStyle->columnRuleWidth());
        }
        else if (isInitial)
            m_style->resetColumnRule();
        return;
    case CSSPropertyWebkitColumns:
        if (isInherit) {
            if (m_parentStyle->hasAutoColumnWidth())
                m_style->setHasAutoColumnWidth();
            else
                m_style->setColumnWidth(m_parentStyle->columnWidth());
            m_style->setColumnCount(m_parentStyle->columnCount());
        } else if (isInitial) {
            m_style->setHasAutoColumnWidth();
            m_style->setColumnCount(RenderStyle::initialColumnCount());
        }
        return;
    case CSSPropertyWebkitMarquee:
        if (valueType != CSSValue::CSS_INHERIT || !m_parentNode) return;
        m_style->setMarqueeDirection(m_parentStyle->marqueeDirection());
        m_style->setMarqueeIncrement(m_parentStyle->marqueeIncrement());
        m_style->setMarqueeSpeed(m_parentStyle->marqueeSpeed());
        m_style->setMarqueeLoopCount(m_parentStyle->marqueeLoopCount());
        m_style->setMarqueeBehavior(m_parentStyle->marqueeBehavior());
        return;
    case CSSPropertyWebkitMarqueeRepetition: {
        HANDLE_INHERIT_AND_INITIAL(marqueeLoopCount, MarqueeLoopCount)
        if (!primitiveValue)
            return;
        if (primitiveValue->getIdent() == CSSValueInfinite)
            m_style->setMarqueeLoopCount(-1); // -1 means repeat forever.
        else if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_NUMBER)
            m_style->setMarqueeLoopCount(primitiveValue->getIntValue());
        return;
    }
    case CSSPropertyWebkitMarqueeSpeed: {
        HANDLE_INHERIT_AND_INITIAL(marqueeSpeed, MarqueeSpeed)     
        if (!primitiveValue)
            return;
        if (primitiveValue->getIdent()) {
            switch (primitiveValue->getIdent()) {
                case CSSValueSlow:
                    m_style->setMarqueeSpeed(500); // 500 msec.
                    break;
                case CSSValueNormal:
                    m_style->setMarqueeSpeed(85); // 85msec. The WinIE default.
                    break;
                case CSSValueFast:
                    m_style->setMarqueeSpeed(10); // 10msec. Super fast.
                    break;
            }
        }
        else if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_S)
            m_style->setMarqueeSpeed(1000 * primitiveValue->getIntValue());
        else if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_MS)
            m_style->setMarqueeSpeed(primitiveValue->getIntValue());
        else if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_NUMBER) // For scrollamount support.
            m_style->setMarqueeSpeed(primitiveValue->getIntValue());
        return;
    }
    case CSSPropertyWebkitMarqueeIncrement: {
        HANDLE_INHERIT_AND_INITIAL(marqueeIncrement, MarqueeIncrement)
        if (!primitiveValue)
            return;
        if (primitiveValue->getIdent()) {
            switch (primitiveValue->getIdent()) {
                case CSSValueSmall:
                    m_style->setMarqueeIncrement(Length(1, Fixed)); // 1px.
                    break;
                case CSSValueNormal:
                    m_style->setMarqueeIncrement(Length(6, Fixed)); // 6px. The WinIE default.
                    break;
                case CSSValueLarge:
                    m_style->setMarqueeIncrement(Length(36, Fixed)); // 36px.
                    break;
            }
        }
        else {
            bool ok = true;
            Length l = convertToLength(primitiveValue, style(), &ok);
            if (ok)
                m_style->setMarqueeIncrement(l);
        }
        return;
    }
    case CSSPropertyWebkitMarqueeStyle: {
        HANDLE_INHERIT_AND_INITIAL(marqueeBehavior, MarqueeBehavior)      
        if (primitiveValue)
            m_style->setMarqueeBehavior(*primitiveValue);
        return;
    }
    case CSSPropertyWebkitMarqueeDirection: {
        HANDLE_INHERIT_AND_INITIAL(marqueeDirection, MarqueeDirection)
        if (primitiveValue)
            m_style->setMarqueeDirection(*primitiveValue);
        return;
    }
    case CSSPropertyWebkitUserDrag: {
        HANDLE_INHERIT_AND_INITIAL(userDrag, UserDrag)      
        if (primitiveValue)
            m_style->setUserDrag(*primitiveValue);
        return;
    }
    case CSSPropertyWebkitUserModify: {
        HANDLE_INHERIT_AND_INITIAL(userModify, UserModify)      
        if (primitiveValue)
            m_style->setUserModify(*primitiveValue);
        return;
    }
    case CSSPropertyWebkitUserSelect: {
        HANDLE_INHERIT_AND_INITIAL(userSelect, UserSelect)      
        if (primitiveValue)
            m_style->setUserSelect(*primitiveValue);
        return;
    }
    case CSSPropertyTextOverflow: {
        // This property is supported by WinIE, and so we leave off the "-webkit-" in order to
        // work with WinIE-specific pages that use the property.
        HANDLE_INHERIT_AND_INITIAL(textOverflow, TextOverflow)
        if (!primitiveValue || !primitiveValue->getIdent())
            return;
        m_style->setTextOverflow(primitiveValue->getIdent() == CSSValueEllipsis);
        return;
    }
    case CSSPropertyWebkitMarginCollapse: {
        if (isInherit) {
            m_style->setMarginTopCollapse(m_parentStyle->marginTopCollapse());
            m_style->setMarginBottomCollapse(m_parentStyle->marginBottomCollapse());
        }
        else if (isInitial) {
            m_style->setMarginTopCollapse(MCOLLAPSE);
            m_style->setMarginBottomCollapse(MCOLLAPSE);
        }
        return;
    }
    case CSSPropertyWebkitMarginTopCollapse: {
        HANDLE_INHERIT_AND_INITIAL(marginTopCollapse, MarginTopCollapse)
        if (primitiveValue)
            m_style->setMarginTopCollapse(*primitiveValue);
        return;
    }
    case CSSPropertyWebkitMarginBottomCollapse: {
        HANDLE_INHERIT_AND_INITIAL(marginBottomCollapse, MarginBottomCollapse)
        if (primitiveValue)
            m_style->setMarginBottomCollapse(*primitiveValue);
        return;
    }

    // Apple-specific changes.  Do not merge these properties into KHTML.
    case CSSPropertyWebkitLineClamp: {
        HANDLE_INHERIT_AND_INITIAL(lineClamp, LineClamp)
        if (!primitiveValue)
            return;
        m_style->setLineClamp(primitiveValue->getIntValue(CSSPrimitiveValue::CSS_PERCENTAGE));
        return;
    }
    case CSSPropertyWebkitHighlight: {
        HANDLE_INHERIT_AND_INITIAL(highlight, Highlight);
        if (primitiveValue->getIdent() == CSSValueNone)
            m_style->setHighlight(nullAtom);
        else
            m_style->setHighlight(primitiveValue->getStringValue());
        return;
    }
    case CSSPropertyWebkitBorderFit: {
        HANDLE_INHERIT_AND_INITIAL(borderFit, BorderFit);
        if (primitiveValue->getIdent() == CSSValueBorder)
            m_style->setBorderFit(BorderFitBorder);
        else
            m_style->setBorderFit(BorderFitLines);
        return;
    }
    case CSSPropertyWebkitTextSizeAdjust: {
        HANDLE_INHERIT_AND_INITIAL(textSizeAdjust, TextSizeAdjust)
        if (!primitiveValue || !primitiveValue->getIdent()) return;
        m_style->setTextSizeAdjust(primitiveValue->getIdent() == CSSValueAuto);
        m_fontDirty = true;
        return;
    }
    case CSSPropertyWebkitTextSecurity: {
        HANDLE_INHERIT_AND_INITIAL(textSecurity, TextSecurity)
        if (primitiveValue)
            m_style->setTextSecurity(*primitiveValue);
        return;
    }
#if ENABLE(DASHBOARD_SUPPORT)
    case CSSPropertyWebkitDashboardRegion: {
        HANDLE_INHERIT_AND_INITIAL(dashboardRegions, DashboardRegions)
        if (!primitiveValue)
            return;

        if (primitiveValue->getIdent() == CSSValueNone) {
            m_style->setDashboardRegions(RenderStyle::noneDashboardRegions());
            return;
        }

        DashboardRegion *region = primitiveValue->getDashboardRegionValue();
        if (!region)
            return;
            
        DashboardRegion *first = region;
        while (region) {
            Length top = convertToLength(region->top(), style());
            Length right = convertToLength(region->right(), style());
            Length bottom = convertToLength(region->bottom(), style());
            Length left = convertToLength(region->left(), style());
            if (region->m_isCircle)
                m_style->setDashboardRegion(StyleDashboardRegion::Circle, region->m_label, top, right, bottom, left, region == first ? false : true);
            else if (region->m_isRectangle)
                m_style->setDashboardRegion(StyleDashboardRegion::Rectangle, region->m_label, top, right, bottom, left, region == first ? false : true);
            region = region->m_next.get();
        }
        
        m_element->document()->setHasDashboardRegions(true);
        
        return;
    }
#else
    case CSSPropertyWebkitDashboardRegion:
        break;
#endif        
    case CSSPropertyWebkitRtlOrdering:
        HANDLE_INHERIT_AND_INITIAL(visuallyOrdered, VisuallyOrdered)
        if (!primitiveValue || !primitiveValue->getIdent())
            return;
        m_style->setVisuallyOrdered(primitiveValue->getIdent() == CSSValueVisual);
        return;
    case CSSPropertyWebkitTextStrokeWidth: {
        HANDLE_INHERIT_AND_INITIAL(textStrokeWidth, TextStrokeWidth)
        float width = 0;
        switch (primitiveValue->getIdent()) {
            case CSSValueThin:
            case CSSValueMedium:
            case CSSValueThick: {
                double result = 1.0 / 48;
                if (primitiveValue->getIdent() == CSSValueMedium)
                    result *= 3;
                else if (primitiveValue->getIdent() == CSSValueThick)
                    result *= 5;
                width = CSSPrimitiveValue::create(result, CSSPrimitiveValue::CSS_EMS)->computeLengthFloat(style(), zoomFactor);
                break;
            }
            default:
                width = primitiveValue->computeLengthFloat(style(), zoomFactor);
                break;
        }
        m_style->setTextStrokeWidth(width);
        return;
    }
    case CSSPropertyWebkitTransform: {
        HANDLE_INHERIT_AND_INITIAL(transform, Transform);
        TransformOperations operations;
        if (!value->isPrimitiveValue()) {
            CSSValueList* list = static_cast<CSSValueList*>(value);
            unsigned size = list->length();
            for (unsigned i = 0; i < size; i++) {
                CSSTransformValue* val = static_cast<CSSTransformValue*>(list->itemWithoutBoundsCheck(i));
                CSSValueList* values = val->values();
                
                CSSPrimitiveValue* firstValue = static_cast<CSSPrimitiveValue*>(values->itemWithoutBoundsCheck(0));
                 
                switch (val->type()) {
                    case CSSTransformValue::ScaleTransformOperation:
                    case CSSTransformValue::ScaleXTransformOperation:
                    case CSSTransformValue::ScaleYTransformOperation: {
                        double sx = 1.0;
                        double sy = 1.0;
                        if (val->type() == CSSTransformValue::ScaleYTransformOperation)
                            sy = firstValue->getDoubleValue();
                        else { 
                            sx = firstValue->getDoubleValue();
                            if (val->type() == CSSTransformValue::ScaleTransformOperation) {
                                if (values->length() > 1) {
                                    CSSPrimitiveValue* secondValue = static_cast<CSSPrimitiveValue*>(values->itemWithoutBoundsCheck(1));
                                    sy = secondValue->getDoubleValue();
                                } else 
                                    sy = sx;
                            }
                        }
                        operations.append(ScaleTransformOperation::create(sx, sy));
                        break;
                    }
                    case CSSTransformValue::TranslateTransformOperation:
                    case CSSTransformValue::TranslateXTransformOperation:
                    case CSSTransformValue::TranslateYTransformOperation: {
                        bool ok;
                        Length tx = Length(0, Fixed);
                        Length ty = Length(0, Fixed);
                        if (val->type() == CSSTransformValue::TranslateYTransformOperation)
                            ty = convertToLength(firstValue, style(), &ok);
                        else { 
                            tx = convertToLength(firstValue, style(), &ok);
                            if (val->type() == CSSTransformValue::TranslateTransformOperation) {
                                if (values->length() > 1) {
                                    CSSPrimitiveValue* secondValue = static_cast<CSSPrimitiveValue*>(values->itemWithoutBoundsCheck(1));
                                    ty = convertToLength(secondValue, style(), &ok);
                                }
                            }
                        }
                        operations.append(TranslateTransformOperation::create(tx, ty));
                        break;
                    }
                    case CSSTransformValue::RotateTransformOperation: {
                        double angle = firstValue->getDoubleValue();
                        if (firstValue->primitiveType() == CSSPrimitiveValue::CSS_RAD)
                            angle = rad2deg(angle);
                        else if (firstValue->primitiveType() == CSSPrimitiveValue::CSS_GRAD)
                            angle = grad2deg(angle);
                        operations.append(RotateTransformOperation::create(angle));
                        break;
                    }
                    case CSSTransformValue::SkewTransformOperation:
                    case CSSTransformValue::SkewXTransformOperation:
                    case CSSTransformValue::SkewYTransformOperation: {
                        double angleX = 0;
                        double angleY = 0;
                        double angle = firstValue->getDoubleValue();
                        if (firstValue->primitiveType() == CSSPrimitiveValue::CSS_RAD)
                            angle = rad2deg(angle);
                        else if (firstValue->primitiveType() == CSSPrimitiveValue::CSS_GRAD)
                            angle = grad2deg(angle);
                        if (val->type() == CSSTransformValue::SkewYTransformOperation)
                            angleY = angle;
                        else {
                            angleX = angle;
                            if (val->type() == CSSTransformValue::SkewTransformOperation) {
                                if (values->length() > 1) {
                                    CSSPrimitiveValue* secondValue = static_cast<CSSPrimitiveValue*>(values->itemWithoutBoundsCheck(1));
                                    angleY = secondValue->getDoubleValue();
                                    if (secondValue->primitiveType() == CSSPrimitiveValue::CSS_RAD)
                                        angleY = rad2deg(angle);
                                    else if (secondValue->primitiveType() == CSSPrimitiveValue::CSS_GRAD)
                                        angleY = grad2deg(angle);
                                } else
                                    angleY = angleX;
                            }
                        }
                        operations.append(SkewTransformOperation::create(angleX, angleY));
                        break;
                    }
                    case CSSTransformValue::MatrixTransformOperation: {
                        double a = firstValue->getDoubleValue();
                        double b = static_cast<CSSPrimitiveValue*>(values->itemWithoutBoundsCheck(1))->getDoubleValue();
                        double c = static_cast<CSSPrimitiveValue*>(values->itemWithoutBoundsCheck(2))->getDoubleValue();
                        double d = static_cast<CSSPrimitiveValue*>(values->itemWithoutBoundsCheck(3))->getDoubleValue();
                        double e = static_cast<CSSPrimitiveValue*>(values->itemWithoutBoundsCheck(4))->getDoubleValue();
                        double f = static_cast<CSSPrimitiveValue*>(values->itemWithoutBoundsCheck(5))->getDoubleValue();
                        operations.append(MatrixTransformOperation::create(a, b, c, d, e, f));
                        break;
                    }   
                    case CSSTransformValue::UnknownTransformOperation:
                        ASSERT_NOT_REACHED();
                        break;
                }
            }
        }
        m_style->setTransform(operations);
        return;
    }
    case CSSPropertyWebkitTransformOrigin:
        HANDLE_INHERIT_AND_INITIAL(transformOriginX, TransformOriginX)
        HANDLE_INHERIT_AND_INITIAL(transformOriginY, TransformOriginY)
        return;
    case CSSPropertyWebkitTransformOriginX: {
        HANDLE_INHERIT_AND_INITIAL(transformOriginX, TransformOriginX)
        CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
        Length l;
        int type = primitiveValue->primitiveType();
        if (type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG)
            l = Length(primitiveValue->computeLengthIntForLength(style(), zoomFactor), Fixed);
        else if (type == CSSPrimitiveValue::CSS_PERCENTAGE)
            l = Length(primitiveValue->getDoubleValue(), Percent);
        else
            return;
        m_style->setTransformOriginX(l);
        break;
    }
    case CSSPropertyWebkitTransformOriginY: {
        HANDLE_INHERIT_AND_INITIAL(transformOriginY, TransformOriginY)
        CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
        Length l;
        int type = primitiveValue->primitiveType();
        if (type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG)
            l = Length(primitiveValue->computeLengthIntForLength(style(), zoomFactor), Fixed);
        else if(type == CSSPrimitiveValue::CSS_PERCENTAGE)
            l = Length(primitiveValue->getDoubleValue(), Percent);
        else
            return;
        m_style->setTransformOriginY(l);
        break;
    }
    case CSSPropertyWebkitTransition:
        if (isInitial)
            m_style->clearTransitions();
        else if (isInherit)
            m_style->inheritTransitions(m_parentStyle->transitions());
        return;
    case CSSPropertyWebkitTransitionDuration:
        HANDLE_TRANSITION_VALUE(duration, Duration, value)
        return;
    case CSSPropertyWebkitTransitionRepeatCount:
        HANDLE_TRANSITION_VALUE(repeatCount, RepeatCount, value)
        return;
    case CSSPropertyWebkitTransitionTimingFunction:
        HANDLE_TRANSITION_VALUE(timingFunction, TimingFunction, value)
        return;
    case CSSPropertyWebkitTransitionProperty:
        HANDLE_TRANSITION_VALUE(property, Property, value)
        return;
    case CSSPropertyInvalid:
        return;
    case CSSPropertyFontStretch:
    case CSSPropertyPage:
    case CSSPropertyQuotes:
    case CSSPropertyScrollbar3dlightColor:
    case CSSPropertyScrollbarArrowColor:
    case CSSPropertyScrollbarDarkshadowColor:
    case CSSPropertyScrollbarFaceColor:
    case CSSPropertyScrollbarHighlightColor:
    case CSSPropertyScrollbarShadowColor:
    case CSSPropertyScrollbarTrackColor:
    case CSSPropertySize:
    case CSSPropertyTextLineThrough:
    case CSSPropertyTextLineThroughColor:
    case CSSPropertyTextLineThroughMode:
    case CSSPropertyTextLineThroughStyle:
    case CSSPropertyTextLineThroughWidth:
    case CSSPropertyTextOverline:
    case CSSPropertyTextOverlineColor:
    case CSSPropertyTextOverlineMode:
    case CSSPropertyTextOverlineStyle:
    case CSSPropertyTextOverlineWidth:
    case CSSPropertyTextUnderline:
    case CSSPropertyTextUnderlineColor:
    case CSSPropertyTextUnderlineMode:
    case CSSPropertyTextUnderlineStyle:
    case CSSPropertyTextUnderlineWidth:
    case CSSPropertyWebkitFontSizeDelta:
    case CSSPropertyWebkitMarginStart:
    case CSSPropertyWebkitPaddingStart:
    case CSSPropertyWebkitTextDecorationsInEffect:
    case CSSPropertyWebkitTextStroke:
        return;
#if ENABLE(SVG)
    default:
        // Try the SVG properties
        applySVGProperty(id, value);
#endif
    }
}

void CSSStyleSelector::mapFillAttachment(FillLayer* layer, CSSValue* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        layer->setAttachment(FillLayer::initialFillAttachment(layer->type()));
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    switch (primitiveValue->getIdent()) {
        case CSSValueFixed:
            layer->setAttachment(false);
            break;
        case CSSValueScroll:
            layer->setAttachment(true);
            break;
        default:
            return;
    }
}

void CSSStyleSelector::mapFillClip(FillLayer* layer, CSSValue* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        layer->setClip(FillLayer::initialFillClip(layer->type()));
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    layer->setClip(*primitiveValue);
}

void CSSStyleSelector::mapFillComposite(FillLayer* layer, CSSValue* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        layer->setComposite(FillLayer::initialFillComposite(layer->type()));
        return;
    }
   
    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    layer->setComposite(*primitiveValue);
}

void CSSStyleSelector::mapFillOrigin(FillLayer* layer, CSSValue* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        layer->setOrigin(FillLayer::initialFillOrigin(layer->type()));
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    layer->setOrigin(*primitiveValue);
}

StyleImage* CSSStyleSelector::styleImage(CSSValue* value)
{
    if (value->isImageValue())
        return static_cast<CSSImageValue*>(value)->cachedImage(m_element->document()->docLoader());
    if (value->isImageGeneratorValue())
        return static_cast<CSSImageGeneratorValue*>(value)->generatedImage();
    return 0;
}

void CSSStyleSelector::mapFillImage(FillLayer* layer, CSSValue* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        layer->setImage(FillLayer::initialFillImage(layer->type()));
        return;
    }

    layer->setImage(styleImage(value));
}

void CSSStyleSelector::mapFillRepeat(FillLayer* layer, CSSValue* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        layer->setRepeat(FillLayer::initialFillRepeat(layer->type()));
        return;
    }
   
    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    layer->setRepeat(*primitiveValue);
}

void CSSStyleSelector::mapFillSize(FillLayer* layer, CSSValue* value)
{
    LengthSize b = FillLayer::initialFillSize(layer->type());
   
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        layer->setSize(b);
        return;
    }
   
    if (!value->isPrimitiveValue())
        return;
       
    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    Pair* pair = primitiveValue->getPairValue();
    if (!pair)
        return;
   
    CSSPrimitiveValue* first = static_cast<CSSPrimitiveValue*>(pair->first());
    CSSPrimitiveValue* second = static_cast<CSSPrimitiveValue*>(pair->second());
   
    if (!first || !second)
        return;
       
    Length firstLength, secondLength;
    int firstType = first->primitiveType();
    int secondType = second->primitiveType();
   
    float zoomFactor = m_style->effectiveZoom();

    if (firstType == CSSPrimitiveValue::CSS_UNKNOWN)
        firstLength = Length(Auto);
    else if (firstType > CSSPrimitiveValue::CSS_PERCENTAGE && firstType < CSSPrimitiveValue::CSS_DEG)
        firstLength = Length(first->computeLengthIntForLength(style(), zoomFactor), Fixed);
    else if (firstType == CSSPrimitiveValue::CSS_PERCENTAGE)
        firstLength = Length(first->getDoubleValue(), Percent);
    else
        return;

    if (secondType == CSSPrimitiveValue::CSS_UNKNOWN)
        secondLength = Length(Auto);
    else if (secondType > CSSPrimitiveValue::CSS_PERCENTAGE && secondType < CSSPrimitiveValue::CSS_DEG)
        secondLength = Length(second->computeLengthIntForLength(style(), zoomFactor), Fixed);
    else if (secondType == CSSPrimitiveValue::CSS_PERCENTAGE)
        secondLength = Length(second->getDoubleValue(), Percent);
    else
        return;
   
    b.width = firstLength;
    b.height = secondLength;
    layer->setSize(b);
}

void CSSStyleSelector::mapFillXPosition(FillLayer* layer, CSSValue* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        layer->setXPosition(FillLayer::initialFillXPosition(layer->type()));
        return;
    }
   
    if (!value->isPrimitiveValue())
        return;

    float zoomFactor = m_style->effectiveZoom();

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    Length l;
    int type = primitiveValue->primitiveType();
    if (type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG)
        l = Length(primitiveValue->computeLengthIntForLength(style(), zoomFactor), Fixed);
    else if (type == CSSPrimitiveValue::CSS_PERCENTAGE)
        l = Length(primitiveValue->getDoubleValue(), Percent);
    else
        return;
    layer->setXPosition(l);
}

void CSSStyleSelector::mapFillYPosition(FillLayer* layer, CSSValue* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        layer->setYPosition(FillLayer::initialFillYPosition(layer->type()));
        return;
    }
   
    if (!value->isPrimitiveValue())
        return;

    float zoomFactor = m_style->effectiveZoom();
   
    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    Length l;
    int type = primitiveValue->primitiveType();
    if (type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG)
        l = Length(primitiveValue->computeLengthIntForLength(style(), zoomFactor), Fixed);
    else if (type == CSSPrimitiveValue::CSS_PERCENTAGE)
        l = Length(primitiveValue->getDoubleValue(), Percent);
    else
        return;
    layer->setYPosition(l);
}

void CSSStyleSelector::mapTransitionDuration(Transition* transition, CSSValue* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        transition->setDuration(RenderStyle::initialTransitionDuration());
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_S)
        transition->setDuration(int(1000*primitiveValue->getFloatValue()));
    else if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_MS)
        transition->setDuration(int(primitiveValue->getFloatValue()));
}

void CSSStyleSelector::mapTransitionRepeatCount(Transition* transition, CSSValue* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        transition->setRepeatCount(RenderStyle::initialTransitionRepeatCount());
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    if (primitiveValue->getIdent() == CSSValueInfinite)
        transition->setRepeatCount(-1);
    else
        transition->setRepeatCount(int(primitiveValue->getFloatValue()));
}

void CSSStyleSelector::mapTransitionTimingFunction(Transition* transition, CSSValue* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        transition->setTimingFunction(RenderStyle::initialTransitionTimingFunction());
        return;
    }

    if (value->isPrimitiveValue()) {
        CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
        switch (primitiveValue->getIdent()) {
            case CSSValueLinear:
                transition->setTimingFunction(TimingFunction(LinearTimingFunction));
                break;
            case CSSValueEase:
                transition->setTimingFunction(TimingFunction());
                break;
            case CSSValueEaseIn:
                transition->setTimingFunction(TimingFunction(CubicBezierTimingFunction, .42, .0, 1.0, 1.0));
                break;
            case CSSValueEaseOut:
                transition->setTimingFunction(TimingFunction(CubicBezierTimingFunction, .0, .0, .58, 1.0));
                break;
            case CSSValueEaseInOut:
                transition->setTimingFunction(TimingFunction(CubicBezierTimingFunction, .42, .0, .58, 1.0));
                break;
        }
        return;
    }

    if (value->isTransitionTimingFunctionValue()) {
        CSSTimingFunctionValue* timingFunction = static_cast<CSSTimingFunctionValue*>(value);
        transition->setTimingFunction(TimingFunction(CubicBezierTimingFunction, timingFunction->x1(), timingFunction->y1(), timingFunction->x2(), timingFunction->y2()));
    }
}

void CSSStyleSelector::mapTransitionProperty(Transition* transition, CSSValue* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        transition->setProperty(RenderStyle::initialTransitionProperty());
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
    transition->setProperty(primitiveValue->getIdent());
}

void CSSStyleSelector::mapNinePieceImage(CSSValue* value, NinePieceImage& image)
{
    // If we're a primitive value, then we are "none" and don't need to alter the empty image at all.
    if (!value || value->isPrimitiveValue())
        return;

    // Retrieve the border image value.
    CSSBorderImageValue* borderImage = static_cast<CSSBorderImageValue*>(value);
   
    // Set the image (this kicks off the load).
    image.m_image = styleImage(borderImage->imageValue());

    // Set up a length box to represent our image slices.
    LengthBox& l = image.m_slices;
    Rect* r = borderImage->m_imageSliceRect.get();
    if (r->top()->primitiveType() == CSSPrimitiveValue::CSS_PERCENTAGE)
        l.top = Length(r->top()->getDoubleValue(), Percent);
    else
        l.top = Length(r->top()->getIntValue(CSSPrimitiveValue::CSS_NUMBER), Fixed);
    if (r->bottom()->primitiveType() == CSSPrimitiveValue::CSS_PERCENTAGE)
        l.bottom = Length(r->bottom()->getDoubleValue(), Percent);
    else
        l.bottom = Length((int)r->bottom()->getFloatValue(CSSPrimitiveValue::CSS_NUMBER), Fixed);
    if (r->left()->primitiveType() == CSSPrimitiveValue::CSS_PERCENTAGE)
        l.left = Length(r->left()->getDoubleValue(), Percent);
    else
        l.left = Length(r->left()->getIntValue(CSSPrimitiveValue::CSS_NUMBER), Fixed);
    if (r->right()->primitiveType() == CSSPrimitiveValue::CSS_PERCENTAGE)
        l.right = Length(r->right()->getDoubleValue(), Percent);
    else
        l.right = Length(r->right()->getIntValue(CSSPrimitiveValue::CSS_NUMBER), Fixed);
   
    // Set the appropriate rules for stretch/round/repeat of the slices
    switch (borderImage->m_horizontalSizeRule) {
        case CSSValueStretch:
            image.m_horizontalRule = StretchImageRule;
            break;
        case CSSValueRound:
            image.m_horizontalRule = RoundImageRule;
            break;
        default: // CSSValueRepeat
            image.m_horizontalRule = RepeatImageRule;
            break;
    }

    switch (borderImage->m_verticalSizeRule) {
        case CSSValueStretch:
            image.m_verticalRule = StretchImageRule;
            break;
        case CSSValueRound:
            image.m_verticalRule = RoundImageRule;
            break;
        default: // CSSValueRepeat
            image.m_verticalRule = RepeatImageRule;
            break;
    }
}

void CSSStyleSelector::checkForTextSizeAdjust()
{
    if (m_style->textSizeAdjust())
        return;
 
    FontDescription newFontDescription(m_style->fontDescription());
    newFontDescription.setComputedSize(newFontDescription.specifiedSize());
    m_style->setFontDescription(newFontDescription);
}

void CSSStyleSelector::checkForZoomChange(RenderStyle* style, RenderStyle* parentStyle)
{
    if (style->effectiveZoom() == parentStyle->effectiveZoom())
        return;
   
    const FontDescription& childFont = style->fontDescription();
    FontDescription newFontDescription(childFont);
    setFontSize(newFontDescription, childFont.specifiedSize());
    style->setFontDescription(newFontDescription);
}

void CSSStyleSelector::checkForGenericFamilyChange(RenderStyle* style, RenderStyle* parentStyle)
{
    const FontDescription& childFont = style->fontDescription();
 
    if (childFont.isAbsoluteSize() || !parentStyle)
        return;

    const FontDescription& parentFont = parentStyle->fontDescription();

    if (childFont.genericFamily() == parentFont.genericFamily())
        return;

    // For now, lump all families but monospace together.
    if (childFont.genericFamily() != FontDescription::MonospaceFamily &&
        parentFont.genericFamily() != FontDescription::MonospaceFamily)
        return;

    // We know the parent is monospace or the child is monospace, and that font
    // size was unspecified.  We want to scale our font size as appropriate.
    // If the font uses a keyword size, then we refetch from the table rather than
    // multiplying by our scale factor.
    float size;
    if (childFont.keywordSize()) {
        size = fontSizeForKeyword(CSSValueXxSmall + childFont.keywordSize() - 1, style->htmlHacks(),
                                  childFont.genericFamily() == FontDescription::MonospaceFamily);
    } else {
        Settings* settings = m_checker.m_document->settings();
        float fixedScaleFactor = settings
            ? static_cast<float>(settings->defaultFixedFontSize()) / settings->defaultFontSize()
            : 1;
        size = (parentFont.genericFamily() == FontDescription::MonospaceFamily) ? 
                childFont.specifiedSize()/fixedScaleFactor :
                childFont.specifiedSize()*fixedScaleFactor;
    }

    FontDescription newFontDescription(childFont);
    setFontSize(newFontDescription, size);
    style->setFontDescription(newFontDescription);
}

void CSSStyleSelector::setFontSize(FontDescription& fontDescription, float size)
{
    fontDescription.setSpecifiedSize(size);
    fontDescription.setComputedSize(getComputedSizeFromSpecifiedSize(fontDescription.isAbsoluteSize(), size));
}

float CSSStyleSelector::getComputedSizeFromSpecifiedSize(bool isAbsoluteSize, float specifiedSize)
{
    // We support two types of minimum font size.  The first is a hard override that applies to
    // all fonts.  This is "minSize."  The second type of minimum font size is a "smart minimum"
    // that is applied only when the Web page can't know what size it really asked for, e.g.,
    // when it uses logical sizes like "small" or expresses the font-size as a percentage of
    // the user's default font setting.

    // With the smart minimum, we never want to get smaller than the minimum font size to keep fonts readable.
    // However we always allow the page to set an explicit pixel size that is smaller,
    // since sites will mis-render otherwise (e.g., http://www.gamespot.com with a 9px minimum).
    
    Settings* settings = m_checker.m_document->settings();
    if (!settings)
        return 1.0f;

    int minSize = settings->minimumFontSize();
    int minLogicalSize = settings->minimumLogicalFontSize();

    float zoomFactor = m_style->effectiveZoom();
    if (m_checker.m_document->frame() && m_checker.m_document->frame()->shouldApplyTextZoom())
        zoomFactor *= m_checker.m_document->frame()->textZoomFactor();

    float zoomedSize = specifiedSize * zoomFactor;

    // Apply the hard minimum first.  We only apply the hard minimum if after zooming we're still too small.
    if (zoomedSize < minSize)
        zoomedSize = minSize;

    // Now apply the "smart minimum."  This minimum is also only applied if we're still too small
    // after zooming.  The font size must either be relative to the user default or the original size
    // must have been acceptable.  In other words, we only apply the smart minimum whenever we're positive
    // doing so won't disrupt the layout.
    if (zoomedSize < minLogicalSize && (specifiedSize >= minLogicalSize || !isAbsoluteSize))
        zoomedSize = minLogicalSize;
    
    // Also clamp to a reasonable maximum to prevent insane font sizes from causing crashes on various
    // platforms (I'm looking at you, Windows.)
    return min(1000000.0f, max(zoomedSize, 1.0f));
}

const int fontSizeTableMax = 16;
const int fontSizeTableMin = 9;
const int totalKeywords = 8;

// WinIE/Nav4 table for font sizes.  Designed to match the legacy font mapping system of HTML.
static const int quirksFontSizeTable[fontSizeTableMax - fontSizeTableMin + 1][totalKeywords] =
{
      { 9,    9,     9,     9,    11,    14,    18,    28 },
      { 9,    9,     9,    10,    12,    15,    20,    31 },
      { 9,    9,     9,    11,    13,    17,    22,    34 },
      { 9,    9,    10,    12,    14,    18,    24,    37 },
      { 9,    9,    10,    13,    16,    20,    26,    40 }, // fixed font default (13)
      { 9,    9,    11,    14,    17,    21,    28,    42 },
      { 9,   10,    12,    15,    17,    23,    30,    45 },
      { 9,   10,    13,    16,    18,    24,    32,    48 }  // proportional font default (16)
};
// HTML       1      2      3      4      5      6      7
// CSS  xxs   xs     s      m      l     xl     xxl
//                          |
//                      user pref

// Strict mode table matches MacIE and Mozilla's settings exactly.
static const int strictFontSizeTable[fontSizeTableMax - fontSizeTableMin + 1][totalKeywords] =
{
      { 9,    9,     9,     9,    11,    14,    18,    27 },
      { 9,    9,     9,    10,    12,    15,    20,    30 },
      { 9,    9,    10,    11,    13,    17,    22,    33 },
      { 9,    9,    10,    12,    14,    18,    24,    36 },
      { 9,   10,    12,    13,    16,    20,    26,    39 }, // fixed font default (13)
      { 9,   10,    12,    14,    17,    21,    28,    42 },
      { 9,   10,    13,    15,    18,    23,    30,    45 },
      { 9,   10,    13,    16,    18,    24,    32,    48 }  // proportional font default (16)
};
// HTML       1      2      3      4      5      6      7
// CSS  xxs   xs     s      m      l     xl     xxl
//                          |
//                      user pref

// For values outside the range of the table, we use Todd Fahrner's suggested scale
// factors for each keyword value.
static const float fontSizeFactors[totalKeywords] = { 0.60f, 0.75f, 0.89f, 1.0f, 1.2f, 1.5f, 2.0f, 3.0f };

float CSSStyleSelector::fontSizeForKeyword(int keyword, bool quirksMode, bool fixed) const
{
    Settings* settings = m_checker.m_document->settings();
    if (!settings)
        return 1.0f;

    int mediumSize = fixed ? settings->defaultFixedFontSize() : settings->defaultFontSize();
    if (mediumSize >= fontSizeTableMin && mediumSize <= fontSizeTableMax) {
        // Look up the entry in the table.
        int row = mediumSize - fontSizeTableMin;
        int col = (keyword - CSSValueXxSmall);
        return quirksMode ? quirksFontSizeTable[row][col] : strictFontSizeTable[row][col];
    }
   
    // Value is outside the range of the table. Apply the scale factor instead.
    float minLogicalSize = max(settings->minimumLogicalFontSize(), 1);
    return max(fontSizeFactors[keyword - CSSValueXxSmall]*mediumSize, minLogicalSize);
}

float CSSStyleSelector::largerFontSize(float size, bool quirksMode) const
{
    // FIXME: Figure out where we fall in the size ranges (xx-small to xxx-large) and scale up to
    // the next size level. 
    return size * 1.2f;
}

float CSSStyleSelector::smallerFontSize(float size, bool quirksMode) const
{
    // FIXME: Figure out where we fall in the size ranges (xx-small to xxx-large) and scale down to
    // the next size level.
    return size / 1.2f;
}

static Color colorForCSSValue(int cssValueId)
{
    struct ColorValue {
        int cssValueId;
        RGBA32 color;
    };

    static const ColorValue colorValues[] = {
        { CSSValueAqua, 0xFF00FFFF },
        { CSSValueBlack, 0xFF000000 },
        { CSSValueBlue, 0xFF0000FF },
        { CSSValueFuchsia, 0xFFFF00FF },
        { CSSValueGray, 0xFF808080 },
        { CSSValueGreen, 0xFF008000  },
        { CSSValueGrey, 0xFF808080 },
        { CSSValueLime, 0xFF00FF00 },
        { CSSValueMaroon, 0xFF800000 },
        { CSSValueNavy, 0xFF000080 },
        { CSSValueOlive, 0xFF808000  },
        { CSSValueOrange, 0xFFFFA500 },
        { CSSValuePurple, 0xFF800080 },
        { CSSValueRed, 0xFFFF0000 },
        { CSSValueSilver, 0xFFC0C0C0 },
        { CSSValueTeal, 0xFF008080  },
        { CSSValueTransparent, 0x00000000 },
        { CSSValueWhite, 0xFFFFFFFF },
        { CSSValueYellow, 0xFFFFFF00 },
        { 0, 0 }
    };

    for (const ColorValue* col = colorValues; col->cssValueId; ++col) {
        if (col->cssValueId == cssValueId)
            return col->color;
    }
    return theme()->systemColor(cssValueId);
}

Color CSSStyleSelector::getColorFromPrimitiveValue(CSSPrimitiveValue* primitiveValue)
{
    Color col;
    int ident = primitiveValue->getIdent();
    if (ident) {
        if (ident == CSSValueWebkitText)
            col = m_element->document()->textColor();
        else if (ident == CSSValueWebkitLink) {
            const Color& linkColor = m_element->document()->linkColor();
            const Color& visitedColor = m_element->document()->visitedLinkColor();
            if (linkColor == visitedColor)
                col = linkColor;
            else {
                if (pseudoState == PseudoUnknown || pseudoState == PseudoAnyLink)
                    pseudoState = m_checker.checkPseudoState(m_element);
                col = (pseudoState == PseudoLink) ? linkColor : visitedColor;
            }
        } else if (ident == CSSValueWebkitActivelink)
            col = m_element->document()->activeLinkColor();
        else if (ident == CSSValueWebkitFocusRingColor)
            col = focusRingColor();
        else if (ident == CSSValueCurrentcolor)
            col = m_style->color();
        else
            col = colorForCSSValue(ident);
    } else if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_RGBCOLOR)
        col.setRGB(primitiveValue->getRGBColorValue());
    return col;
}

bool CSSStyleSelector::hasSelectorForAttribute(const AtomicString &attrname)
{
    return m_selectorAttrs.contains(attrname.impl());
}

void CSSStyleSelector::addViewportDependentMediaQueryResult(const MediaQueryExp* expr, bool result)
{
    m_viewportDependentMediaQueryResults.append(new MediaQueryResult(*expr, result));
}

bool CSSStyleSelector::affectedByViewportChange() const
{
    unsigned s = m_viewportDependentMediaQueryResults.size();
    for (unsigned i = 0; i < s; i++) {
        if (m_medium->eval(&m_viewportDependentMediaQueryResults[i]->m_expression) != m_viewportDependentMediaQueryResults[i]->m_result)
            return true;
    }
    return false;
}

void CSSStyleSelector::SelectorChecker::allVisitedStateChanged()
{
    if (m_linksCheckedForVisitedState.isEmpty())
        return;
    for (Node* node = m_document; node; node = node->traverseNextNode()) {
        if (node->isLink())
            node->setChanged();
    }
}

void CSSStyleSelector::SelectorChecker::visitedStateChanged(unsigned visitedHash)
{
    if (!m_linksCheckedForVisitedState.contains(visitedHash))
        return;
    for (Node* node = m_document; node; node = node->traverseNextNode()) {
        const AtomicString* attr = linkAttribute(node);
        if (attr && m_document->visitedLinkHash(*attr) == visitedHash)
            node->setChanged();
    }
}

//+daw ca 24/07 static and global management
void CSSStyleSelector::staticFinalize()
{
	delete defaultStyle;
	defaultStyle = 0;

	delete defaultQuirksStyle;
	defaultQuirksStyle = 0;
	
	delete defaultPrintStyle;
	defaultPrintStyle = 0;
	
	delete defaultViewSourceStyle;
	defaultViewSourceStyle = 0;

	delete defaultSheet;
	defaultSheet = 0;

	delete s_styleNotYetAvailable;
	s_styleNotYetAvailable = 0;

	delete quirksSheet;
	quirksSheet = 0;

	delete viewSourceSheet;
	viewSourceSheet = 0;

    // Added by Paul Pedriana, 1/2009 for memory leak fixes.
    while(gUASheetsCount > 0)
    {
        --gUASheetsCount;
        gUASheets[gUASheetsCount]->deref();
    }

    
    // 3/27/09 CSidhall - Leak fix
    if(sHtmlCaseInsensitiveAttributesSet) {
        delete sHtmlCaseInsensitiveAttributesSet;
        sHtmlCaseInsensitiveAttributesSet = NULL;
    }

    if(spStaticScreenEval) {
        delete spStaticScreenEval;
        spStaticScreenEval = NULL;
    }

    if(spStaticPrintEval) {
        delete spStaticPrintEval;
        spStaticPrintEval = NULL;
    }
}
//daw ca

} // namespace WebCore
