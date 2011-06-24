/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
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
 */

/*
* This file was modified by Electronic Arts Inc Copyright � 2009
*/

#include "config.h"
#include "HTMLScriptElement.h"

#include "CachedScript.h"
#include "DocLoader.h"
#include "Document.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLNames.h"
#include "ScriptController.h"
#include "MIMETypeRegistry.h"
#include "Text.h"

namespace WebCore {

using namespace HTMLNames;

HTMLScriptElement::HTMLScriptElement(Document* doc)
    : HTMLElement(scriptTag, doc)
    , m_cachedScript(0)
    , m_createdByParser(false)
    , m_evaluated(false)
{
}

HTMLScriptElement::~HTMLScriptElement()
{
    if (m_cachedScript)
        m_cachedScript->removeClient(this);
}

bool HTMLScriptElement::isURLAttribute(Attribute* attr) const
{
    return attr->name() == srcAttr;
}

void HTMLScriptElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    // If a node is inserted as a child of the script element
    // and the script element has been inserted in the document
    // we evaluate the script.
    if (!m_createdByParser && inDocument() && firstChild())
        evaluateScript(document()->url().string(), text());
    HTMLElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);
}

void HTMLScriptElement::parseMappedAttribute(MappedAttribute* attr)
{
    const QualifiedName& attrName = attr->name();
    if (attrName == srcAttr) {
        if (m_evaluated || m_cachedScript || m_createdByParser || !inDocument())
            return;

        // FIXME: Evaluate scripts in viewless documents.
        // See http://bugs.webkit.org/show_bug.cgi?id=5727
        if (!document()->frame())
            return;
    
        const AtomicString& url = attr->value();
        if (!url.isEmpty()) {
            m_cachedScript = document()->docLoader()->requestScript(url, scriptCharset());
            if (m_cachedScript)
                m_cachedScript->addClient(this);
            else
                dispatchHTMLEvent(eventNames().errorEvent, true, false);
        }
    } else if (attrName == onloadAttr)
        setHTMLEventListener(eventNames().loadEvent, attr);
    else
        HTMLElement::parseMappedAttribute(attr);
}

void HTMLScriptElement::finishParsingChildren()
{
    // The parser just reached </script>. If we have no src and no text,
    // allow dynamic loading later.
    if (getAttribute(srcAttr).isEmpty() && text().isEmpty())
        setCreatedByParser(false);
    HTMLElement::finishParsingChildren();
}

void HTMLScriptElement::insertedIntoDocument()
{
    HTMLElement::insertedIntoDocument();

    ASSERT(!m_cachedScript);

    if (m_createdByParser)
        return;
    
    // FIXME: Eventually we'd like to evaluate scripts which are inserted into a 
    // viewless document but this'll do for now.
    // See http://bugs.webkit.org/show_bug.cgi?id=5727
    if (!document()->frame())
        return;
    
    const AtomicString& url = getAttribute(srcAttr);
    if (!url.isEmpty()) {
        m_cachedScript = document()->docLoader()->requestScript(url, scriptCharset());
        if (m_cachedScript)
            m_cachedScript->addClient(this);
        else
            dispatchHTMLEvent(eventNames().errorEvent, true, false);
        return;
    }

    // If there's an empty script node, we shouldn't evaluate the script
    // because if a script is inserted afterwards (by setting text or innerText)
    // it should be evaluated, and evaluateScript only evaluates a script once.
    String scriptString = text();    
    if (!scriptString.isEmpty())
        evaluateScript(document()->url().string(), scriptString);
}

void HTMLScriptElement::removedFromDocument()
{
    HTMLElement::removedFromDocument();

    if (m_cachedScript) {
        m_cachedScript->removeClient(this);
        m_cachedScript = 0;
    }
}

void HTMLScriptElement::notifyFinished(CachedResource* o)
{
    CachedScript* cs = static_cast<CachedScript*>(o);

    ASSERT(cs == m_cachedScript);

    // Evaluating the script could lead to a garbage collection which
    // can delete the script element so we need to protect it.
    RefPtr<HTMLScriptElement> protect(this);
    
    if (cs->errorOccurred())
        dispatchHTMLEvent(eventNames().errorEvent, true, false);
    else {
        evaluateScript(cs->url(), cs->script());
        dispatchHTMLEvent(eventNames().loadEvent, false, false);
    }

    // script evaluation may have dereffed it already
    if (m_cachedScript) {
        m_cachedScript->removeClient(this);
        m_cachedScript = 0;
    }
}

bool HTMLScriptElement::shouldExecuteAsJavaScript()
{
    /*
         Mozilla 1.8 accepts javascript1.0 - javascript1.7, but WinIE 7 accepts only javascript1.1 - javascript1.3.
         Mozilla 1.8 and WinIE 7 both accept javascript and livescript.
         WinIE 7 accepts ecmascript and jscript, but Mozilla 1.8 doesn't.
         Neither Mozilla 1.8 nor WinIE 7 accept leading or trailing whitespace.
         We want to accept all the values that either of these browsers accept, but not other values.
     */

    ////////////////
    // Modified by Paul Pedriana, 1/2009
    const AtomicString validLanguages[] = {
        javascriptHTMLScriptElement,
        javascript10HTMLScriptElement,
        javascript11HTMLScriptElement,
        javascript12HTMLScriptElement,
        javascript13HTMLScriptElement,
        javascript14HTMLScriptElement,
        javascript15HTMLScriptElement,
        javascript16HTMLScriptElement,
        javascript17HTMLScriptElement,
        livescriptHTMLScriptElement,
        ecmascriptHTMLScriptElement,
        jscriptHTMLScriptElement
    };
    static const unsigned validLanguagesCount = sizeof(validLanguages) / sizeof(validLanguages[0]); 
    ////////////////

    const AtomicString& type = getAttribute(typeAttr);
    if (!type.isEmpty()) {
        String lowerType = type.string().stripWhiteSpace().lower();
        if (MIMETypeRegistry::isSupportedJavaScriptMIMEType(lowerType))
            return true;

        return false;
    }

    const AtomicString& language = getAttribute(languageAttr);
    if (!language.isEmpty()) {
        String lowerLanguage = language.string().lower();
        for (unsigned i = 0; i < validLanguagesCount; ++i)
            if (lowerLanguage == validLanguages[i])
                return true;

        return false;
    }

    // No type or language is specified, so we assume the script to be JavaScript
    return true;
}

void HTMLScriptElement::evaluateScript(const String& url, const String& script)
{
    if (m_evaluated)
        return;
    
    if (!shouldExecuteAsJavaScript())
        return;
    
    Frame* frame = document()->frame();
    if (frame) {
        if (frame->script()->isEnabled()) {
            m_evaluated = true;
            // FIXME: This starting line number will be incorrect for evaluation triggered
            // from insertedIntoDocument or childrenChanged.
            frame->script()->evaluate(url, 1, script);
            Document::updateDocumentsRendering();
        }
    }
}

String HTMLScriptElement::text() const
{
    Vector<UChar> val;
    Text* firstTextNode = 0;
    bool foundMultipleTextNodes = false;
    
    for (Node* n = firstChild(); n; n = n->nextSibling()) {
        if (n->isTextNode()) {
            if (foundMultipleTextNodes)
                append(val, static_cast<Text*>(n)->data());
            else if (firstTextNode) {
                append(val, firstTextNode->data());
                append(val, static_cast<Text*>(n)->data());
                foundMultipleTextNodes = true;
            } else {
                firstTextNode = static_cast<Text*>(n);
            }
        }
    }
        
    if (firstTextNode && !foundMultipleTextNodes)
        return firstTextNode->data();
    
    return String::adopt(val);
}

void HTMLScriptElement::setText(const String &value)
{
    ExceptionCode ec = 0;
    int numChildren = childNodeCount();
    
    if (numChildren == 1 && firstChild()->isTextNode()) {
        static_cast<Text *>(firstChild())->setData(value, ec);
        return;
    }
    
    if (numChildren > 0)
        removeChildren();
    
    appendChild(document()->createTextNode(value.impl()), ec);
}

String HTMLScriptElement::htmlFor() const
{
    // DOM Level 1 says: reserved for future use.
    return String();
}

void HTMLScriptElement::setHtmlFor(const String& /*value*/)
{
    // DOM Level 1 says: reserved for future use.
}

String HTMLScriptElement::event() const
{
    // DOM Level 1 says: reserved for future use.
    return String();
}

void HTMLScriptElement::setEvent(const String& /*value*/)
{
    // DOM Level 1 says: reserved for future use.
}

String HTMLScriptElement::charset() const
{
    return getAttribute(charsetAttr);
}

void HTMLScriptElement::setCharset(const String &value)
{
    setAttribute(charsetAttr, value);
}

bool HTMLScriptElement::defer() const
{
    return !getAttribute(deferAttr).isNull();
}

void HTMLScriptElement::setDefer(bool defer)
{
    setAttribute(deferAttr, defer ? "" : 0);
}

KURL HTMLScriptElement::src() const
{
    return document()->completeURL(getAttribute(srcAttr));
}

void HTMLScriptElement::setSrc(const String &value)
{
    setAttribute(srcAttr, value);
}

String HTMLScriptElement::type() const
{
    return getAttribute(typeAttr);
}

void HTMLScriptElement::setType(const String &value)
{
    setAttribute(typeAttr, value);
}

String HTMLScriptElement::scriptCharset() const
{
    // First we try to get encoding from charset attribute.
    String charset = getAttribute(charsetAttr).string().stripWhiteSpace();
    // If charset has not been declared in script tag, fall back
    // to frame encoding.
    if (charset.isEmpty()) {
        if (Frame* frame = document()->frame())
            charset = frame->loader()->encoding();
    }
    return charset;
}

void HTMLScriptElement::getSubresourceAttributeStrings(Vector<String>& urls) const
{
    urls.append(src().string());
}

}
