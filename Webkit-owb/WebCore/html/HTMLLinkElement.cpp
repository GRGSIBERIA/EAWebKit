/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2008 Apple Inc. All rights reserved.
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

#include "config.h"
#include "HTMLLinkElement.h"

#include "CSSHelper.h"
#include "CachedCSSStyleSheet.h"
#include "DocLoader.h"
#include "Document.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "HTMLNames.h"
#include "MediaList.h"
#include "MediaQueryEvaluator.h"

namespace WebCore {

using namespace HTMLNames;

HTMLLinkElement::HTMLLinkElement(Document *doc)
    : HTMLElement(linkTag, doc)
    , m_cachedSheet(0)
    , m_disabledState(0)
    , m_loading(false)
    , m_alternate(false)
    , m_isStyleSheet(false)
    , m_isIcon(false)
{
}

HTMLLinkElement::~HTMLLinkElement()
{
    if (m_cachedSheet) {
        m_cachedSheet->removeClient(this);
        if (m_loading && !isDisabled() && !isAlternate())
            document()->removePendingSheet();
    }
}

void HTMLLinkElement::setDisabledState(bool _disabled)
{
    int oldDisabledState = m_disabledState;
    m_disabledState = _disabled ? 2 : 1;
    if (oldDisabledState != m_disabledState) {
        // If we change the disabled state while the sheet is still loading, then we have to
        // perform three checks:
        if (isLoading()) {
            // Check #1: If the sheet becomes disabled while it was loading, and if it was either
            // a main sheet or a sheet that was previously enabled via script, then we need
            // to remove it from the list of pending sheets.
            if (m_disabledState == 2 && (!m_alternate || oldDisabledState == 1))
                document()->removePendingSheet();

            // Check #2: An alternate sheet becomes enabled while it is still loading.
            if (m_alternate && m_disabledState == 1)
                document()->addPendingSheet();

            // Check #3: A main sheet becomes enabled while it was still loading and
            // after it was disabled via script.  It takes really terrible code to make this
            // happen (a double toggle for no reason essentially).  This happens on
            // virtualplastic.net, which manages to do about 12 enable/disables on only 3
            // sheets. :)
            if (!m_alternate && m_disabledState == 1 && oldDisabledState == 2)
                document()->addPendingSheet();

            // If the sheet is already loading just bail.
            return;
        }

        // Load the sheet, since it's never been loaded before.
        if (!m_sheet && m_disabledState == 1)
            process();
        else
            document()->updateStyleSelector(); // Update the style selector.
    }
}

StyleSheet* HTMLLinkElement::sheet() const
{
    return m_sheet.get();
}

void HTMLLinkElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == relAttr) {
        tokenizeRelAttribute(attr->value(), m_isStyleSheet, m_alternate, m_isIcon);
        process();
    } else if (attr->name() == hrefAttr) {
        m_url = document()->completeURL(parseURL(attr->value())).string();
        process();
    } else if (attr->name() == typeAttr) {
        m_type = attr->value();
        process();
    } else if (attr->name() == mediaAttr) {
        m_media = attr->value().string().lower();
        process();
    } else if (attr->name() == disabledAttr) {
        setDisabledState(!attr->isNull());
    } else {
        if (attr->name() == titleAttr && m_sheet)
            m_sheet->setTitle(attr->value());
        HTMLElement::parseMappedAttribute(attr);
    }
}

void HTMLLinkElement::tokenizeRelAttribute(const AtomicString& relStr, bool& styleSheet, bool& alternate, bool& icon)
{
    styleSheet = false;
    icon = false; 
    alternate = false;
    String rel = relStr.string().lower();
    if (rel == "stylesheet")
        styleSheet = true;
    else if (rel == "icon" || rel == "shortcut icon")
        icon = true;
    else if (rel == "alternate stylesheet" || rel == "stylesheet alternate") {
        styleSheet = true;
        alternate = true;
    } else {
        // Tokenize the rel attribute and set bits based on specific keywords that we find.
        rel.replace('\n', ' ');
        Vector<String> list;
        rel.split(' ', list);
        Vector<String>::const_iterator end = list.end();
        for (Vector<String>::const_iterator it = list.begin(); it != end; ++it) {
            if (*it == "stylesheet")
                styleSheet = true;
            else if (*it == "alternate")
                alternate = true;
            else if (*it == "icon")
                icon = true;
        }
    }
}

void HTMLLinkElement::process()
{
    if (!inDocument())
        return;

    String type = m_type.lower();

    // IE extension: location of small icon for locationbar / bookmarks
    // We'll record this URL per document, even if we later only use it in top level frames
    if (m_isIcon && !m_url.isEmpty())
        document()->setIconURL(m_url, type);

    // Stylesheet
    // This was buggy and would incorrectly match <link rel="alternate">, which has a different specified meaning. -dwh
    if (m_disabledState != 2 && m_isStyleSheet && document()->frame()) {
        // no need to load style sheets which aren't for the screen output
        // ### there may be in some situations e.g. for an editor or script to manipulate
        // also, don't load style sheets for standalone documents
        MediaQueryEvaluator allEval(true);
        MediaQueryEvaluator screenEval("screen", true);
        MediaQueryEvaluator printEval("print", true);
        RefPtr<MediaList> media = MediaList::createAllowingDescriptionSyntax(m_media);
        if (allEval.eval(media.get()) || screenEval.eval(media.get()) || printEval.eval(media.get())) {

            // Add ourselves as a pending sheet, but only if we aren't an alternate 
            // stylesheet.  Alternate stylesheets don't hold up render tree construction.
            if (!isAlternate())
                document()->addPendingSheet();

            String chset = getAttribute(charsetAttr);
            if (chset.isEmpty() && document()->frame())
                chset = document()->frame()->loader()->encoding();
            
            if (m_cachedSheet) {
                if (m_loading)
                    document()->removePendingSheet();
                m_cachedSheet->removeClient(this);
            }
            m_loading = true;
            m_cachedSheet = document()->docLoader()->requestCSSStyleSheet(m_url, chset);
            if (m_cachedSheet)
                m_cachedSheet->addClient(this);
            else if (!isAlternate()) { // request may have been denied if stylesheet is local and document is remote.
                m_loading = false;
                document()->removePendingSheet();
            }
        }
    } else if (m_sheet) {
        // we no longer contain a stylesheet, e.g. perhaps rel or type was changed
        m_sheet = 0;
        document()->updateStyleSelector();
    }
}

void HTMLLinkElement::insertedIntoDocument()
{
    HTMLElement::insertedIntoDocument();
    process();
}

void HTMLLinkElement::removedFromDocument()
{
    HTMLElement::removedFromDocument();
    process();
}

void HTMLLinkElement::setCSSStyleSheet(const String& url, const String& charset, const CachedCSSStyleSheet* sheet)
{
    bool strict = !document()->inCompatMode();
    m_sheet = CSSStyleSheet::create(this, url, charset);
    m_sheet->parseString(sheet->sheetText(strict), strict);
    m_sheet->setTitle(title());

    RefPtr<MediaList> media = MediaList::createAllowingDescriptionSyntax(m_media);
    m_sheet->setMedia(media.get());

    m_loading = false;
    m_sheet->checkLoaded();
}

bool HTMLLinkElement::isLoading() const
{
    if (m_loading)
        return true;
    if (!m_sheet)
        return false;
    return static_cast<CSSStyleSheet *>(m_sheet.get())->isLoading();
}

bool HTMLLinkElement::sheetLoaded()
{
    if (!isLoading() && !isDisabled() && !isAlternate()) {
        document()->removePendingSheet();
        return true;
    }
    return false;
}

bool HTMLLinkElement::isURLAttribute(Attribute *attr) const
{
    return attr->name() == hrefAttr;
}

bool HTMLLinkElement::disabled() const
{
    return !getAttribute(disabledAttr).isNull();
}

void HTMLLinkElement::setDisabled(bool disabled)
{
    setAttribute(disabledAttr, disabled ? "" : 0);
}

String HTMLLinkElement::charset() const
{
    return getAttribute(charsetAttr);
}

void HTMLLinkElement::setCharset(const String& value)
{
    setAttribute(charsetAttr, value);
}

KURL HTMLLinkElement::href() const
{
    return document()->completeURL(getAttribute(hrefAttr));
}

void HTMLLinkElement::setHref(const String& value)
{
    setAttribute(hrefAttr, value);
}

String HTMLLinkElement::hreflang() const
{
    return getAttribute(hreflangAttr);
}

void HTMLLinkElement::setHreflang(const String& value)
{
    setAttribute(hreflangAttr, value);
}

String HTMLLinkElement::media() const
{
    return getAttribute(mediaAttr);
}

void HTMLLinkElement::setMedia(const String& value)
{
    setAttribute(mediaAttr, value);
}

String HTMLLinkElement::rel() const
{
    return getAttribute(relAttr);
}

void HTMLLinkElement::setRel(const String& value)
{
    setAttribute(relAttr, value);
}

String HTMLLinkElement::rev() const
{
    return getAttribute(revAttr);
}

void HTMLLinkElement::setRev(const String& value)
{
    setAttribute(revAttr, value);
}

String HTMLLinkElement::target() const
{
    return getAttribute(targetAttr);
}

void HTMLLinkElement::setTarget(const String& value)
{
    setAttribute(targetAttr, value);
}

String HTMLLinkElement::type() const
{
    return getAttribute(typeAttr);
}

void HTMLLinkElement::setType(const String& value)
{
    setAttribute(typeAttr, value);
}

void HTMLLinkElement::getSubresourceAttributeStrings(Vector<String>& urls) const
{    
    if (m_isIcon) {
        urls.append(href().string());
        return;
    }
    
    if (!m_isStyleSheet)
        return;
        
    // Append the URL of this link element.
    urls.append(href().string());
    
    // Walk the URLs linked by the linked-to stylesheet.
    HashSet<String> styleURLs;
    StyleSheet* styleSheet = const_cast<HTMLLinkElement*>(this)->sheet();
    if (styleSheet)
        styleSheet->addSubresourceURLStrings(styleURLs, href());
    
    HashSet<String>::iterator end = styleURLs.end();
    for (HashSet<String>::iterator i = styleURLs.begin(); i != end; ++i)
        urls.append(*i);
}

}
