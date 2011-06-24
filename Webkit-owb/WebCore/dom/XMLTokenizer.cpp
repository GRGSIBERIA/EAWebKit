/*
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
 * Copyright (C) 2005, 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2007 Samuel Weinig (sam@webkit.org)
 * Copyright (C) 2007 Trolltech ASA
 * Copyright (C) 2008 Holger Hans Peter Freyther
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
#include "XMLTokenizer.h"

#include "CDATASection.h"
#include "CString.h"
#include "CachedScript.h"
#include "Comment.h"
#include "DocLoader.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "DocumentType.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "HTMLLinkElement.h"
#include "HTMLNames.h"
#include "HTMLScriptElement.h"
#include "HTMLStyleElement.h"
#include "HTMLTokenizer.h"
#include "ScriptController.h"
#include "ProcessingInstruction.h"
#include "ResourceError.h"
#include "ResourceHandle.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "TextResourceDecoder.h"
#include <wtf/Platform.h>
#include <wtf/StringExtras.h>
#include <wtf/Threading.h>
#include <wtf/Vector.h>

#if ENABLE(SVG)
#include "SVGNames.h"
#include "SVGScriptElement.h"
#include "SVGStyleElement.h"
#endif

using namespace std;

namespace WebCore {

const int maxErrors = 25;

/*
bool isScriptElement(Element* element)
{
    return element->hasTagName(HTMLNames::scriptTag)
#if ENABLE(SVG)
        || element->hasTagName(SVGNames::scriptTag)
#endif
        ;
}

ScriptElement* castToScriptElement(Element* element)
{
    ASSERT(isScriptElement(element));

    if (element->hasTagName(HTMLNames::scriptTag))
        return static_cast<HTMLScriptElement*>(element);

#if ENABLE(SVG)
    if (element->hasTagName(SVGNames::scriptTag))
        return static_cast<SVGScriptElement*>(element);
#endif

    ASSERT_NOT_REACHED();
    return 0;
}
*/

void XMLTokenizer::setCurrentNode(Node* n)
{
    bool nodeNeedsReference = n && n != m_doc;
    if (nodeNeedsReference)
        n->ref(); 
    if (m_currentNodeIsReferenced) 
        m_currentNode->deref(); 
    m_currentNode = n;
    m_currentNodeIsReferenced = nodeNeedsReference;
}

bool XMLTokenizer::write(const SegmentedString& s, bool /*appendData*/)
{
    String parseString = s.toString();
    
    if (m_sawXSLTransform || !m_sawFirstElement)
        m_originalSourceForTransform += parseString;

    if (m_parserStopped || m_sawXSLTransform)
        return false;
    
    if (m_parserPaused) {
        m_pendingSrc.append(s);
        return false;
    }
    
    doWrite(s.toString());
    return false;
}

/*
void XMLTokenizer::eventuallyMarkAsParserCreated(Element* element)
{
    if (element->hasTagName(HTMLNames::scriptTag))
        static_cast<HTMLScriptElement*>(element)->setCreatedByParser(true);
#if ENABLE(SVG)
    else if (element->hasTagName(SVGNames::scriptTag))
        static_cast<SVGScriptElement*>(element)->setCreatedByParser(true);
#endif
    else if (element->hasTagName(HTMLNames::styleTag))
        static_cast<HTMLStyleElement*>(element)->setCreatedByParser(true);
#if ENABLE(SVG)
    else if (element->hasTagName(SVGNames::styleTag))
        static_cast<SVGStyleElement*>(element)->setCreatedByParser(true);
#endif
    else if (element->hasTagName(HTMLNames::linkTag))
        static_cast<HTMLLinkElement*>(element)->setCreatedByParser(true);
}
*/

void XMLTokenizer::handleError(ErrorType type, const char* m, int lineNumber, int columnNumber)
{
    if (type == fatal || (m_errorCount < maxErrors && m_lastErrorLine != lineNumber && m_lastErrorColumn != columnNumber)) {
        switch (type) {
            case warning:
                m_errorMessages += String::format("warning on line %d at column %d: %s", lineNumber, columnNumber, m);
                break;
            case fatal:
            case nonFatal:
                m_errorMessages += String::format("error on line %d at column %d: %s", lineNumber, columnNumber, m);
        }
        
        m_lastErrorLine = lineNumber;
        m_lastErrorColumn = columnNumber;
        ++m_errorCount;
    }
    
    if (type != warning)
        m_sawError = true;
    
    if (type == fatal)
        stopParsing();    
}

bool XMLTokenizer::enterText()
{
#if !USE(QXMLSTREAM) && !USE(UTFXML)  // UTFXml is much like QXml: they are a streaming API as opposed to a SAX API.
    ASSERT(m_bufferedText.size() == 0);
#endif
    RefPtr<Node> newNode = new Text(m_doc, "");
    if (!m_currentNode->addChild(newNode.get()))
        return false;
    setCurrentNode(newNode.get());
    return true;
}

#if !USE(QXMLSTREAM) && !USE(UTFXML)  // UTFXml is much like QXml: they are a streaming API as opposed to a SAX API.
static inline String toString(const xmlChar* str, unsigned len)
{
    return UTF8Encoding().decode(reinterpret_cast<const char*>(str), len);
}
#endif


void XMLTokenizer::exitText()
{
    if (m_parserStopped)
        return;

    if (!m_currentNode || !m_currentNode->isTextNode())
        return;

#if !USE(QXMLSTREAM) && !USE(UTFXML)  // UTFXml is much like QXml: they are a streaming API as opposed to a SAX API.
    ExceptionCode ec = 0;
    static_cast<Text*>(m_currentNode)->appendData(toString(m_bufferedText.data(), m_bufferedText.size()), ec);
    Vector<xmlChar> empty;
    m_bufferedText.swap(empty);
#endif

    if (m_view && m_currentNode && !m_currentNode->attached())
        m_currentNode->attach();

    // FIXME: What's the right thing to do if the parent is really 0?
    // Just leaving the current node set to the text node doesn't make much sense.
    if (Node* par = m_currentNode->parentNode())
        setCurrentNode(par);
}

void XMLTokenizer::end()
{
    doEnd();
    
    if (m_sawError)
        insertErrorMessageBlock();
    else {
        exitText();
        m_doc->updateStyleSelector();
    }
    
    setCurrentNode(0);
    if (!m_parsingFragment)
        m_doc->finishedParsing();    
}

void XMLTokenizer::finish()
{
    if (m_parserPaused)
        m_finishCalled = true;
    else
        end();
}

static inline RefPtr<Element> createXHTMLParserErrorHeader(Document* doc, const String& errorMessages) 
{
    ExceptionCode ec = 0;
    RefPtr<Element> reportElement = doc->createElementNS(HTMLNames::xhtmlNamespaceURI, "parsererror", ec);
    reportElement->setAttribute(HTMLNames::styleAttr, "display: block; white-space: pre; border: 2px solid #c77; padding: 0 1em 0 1em; margin: 1em; background-color: #fdd; color: black");
    
    RefPtr<Element> h3 = doc->createElementNS(HTMLNames::xhtmlNamespaceURI, "h3", ec);
    reportElement->appendChild(h3.get(), ec);
    h3->appendChild(doc->createTextNode("This page contains the following errors:"), ec);
    
    RefPtr<Element> fixed = doc->createElementNS(HTMLNames::xhtmlNamespaceURI, "div", ec);
    reportElement->appendChild(fixed.get(), ec);
    fixed->setAttribute(HTMLNames::styleAttr, "font-family:monospace;font-size:12px");
    fixed->appendChild(doc->createTextNode(errorMessages), ec);
    
    h3 = doc->createElementNS(HTMLNames::xhtmlNamespaceURI, "h3", ec);
    reportElement->appendChild(h3.get(), ec);
    h3->appendChild(doc->createTextNode("Below is a rendering of the page up to the first error."), ec);
    
    return reportElement;
}

void XMLTokenizer::insertErrorMessageBlock()
{
#if USE(QXMLSTREAM) || USE(UTFXML)  // UTFXml is much like QXml: they are a streaming API as opposed to a SAX API.
    if (m_parsingFragment)
        return;
#endif
    // One or more errors occurred during parsing of the code. Display an error block to the user above
    // the normal content (the DOM tree is created manually and includes line/col info regarding 
    // where the errors are located)

    // Create elements for display
    ExceptionCode ec = 0;
    Document* doc = m_doc;
    Node* documentElement = doc->documentElement();
    if (!documentElement) {
        RefPtr<Node> rootElement = doc->createElementNS(HTMLNames::xhtmlNamespaceURI, "html", ec);
        doc->appendChild(rootElement, ec);
        RefPtr<Node> body = doc->createElementNS(HTMLNames::xhtmlNamespaceURI, "body", ec);
        rootElement->appendChild(body, ec);
        documentElement = body.get();
    }
#if ENABLE(SVG)
    else if (documentElement->namespaceURI() == SVGNames::svgNamespaceURI) {
        // Until our SVG implementation has text support, it is best if we 
        // wrap the erroneous SVG document in an xhtml document and render
        // the combined document with error messages.
        RefPtr<Node> rootElement = doc->createElementNS(HTMLNames::xhtmlNamespaceURI, "html", ec);
        RefPtr<Node> body = doc->createElementNS(HTMLNames::xhtmlNamespaceURI, "body", ec);
        rootElement->appendChild(body, ec);
        body->appendChild(documentElement, ec);
        doc->appendChild(rootElement.get(), ec);
        documentElement = body.get();
    }
#endif

    RefPtr<Element> reportElement = createXHTMLParserErrorHeader(doc, m_errorMessages);
    documentElement->insertBefore(reportElement, documentElement->firstChild(), ec);
#if ENABLE(XSLT)
    if (doc->transformSourceDocument()) {
        RefPtr<Element> par = doc->createElementNS(HTMLNames::xhtmlNamespaceURI, "p", ec);
        reportElement->appendChild(par, ec);
        par->setAttribute(HTMLNames::styleAttr, "white-space: normal");
        par->appendChild(doc->createTextNode("This document was created as the result of an XSL transformation. The line and column numbers given are from the transformed result."), ec);
    }
#endif
    doc->updateRendering();
}

void XMLTokenizer::notifyFinished(CachedResource* finishedObj)
{
    ASSERT(m_pendingScript == finishedObj);
    ASSERT(m_pendingScript->accessCount() > 0);
        
    String cachedScriptUrl = m_pendingScript->url();
    String scriptSource = m_pendingScript->script();
    bool errorOccurred = m_pendingScript->errorOccurred();
    m_pendingScript->removeClient(this);
    m_pendingScript = 0;
    
    RefPtr<Element> e = m_scriptElement;
    m_scriptElement = 0;
    
    if (errorOccurred) 
        EventTargetNodeCast(e.get())->dispatchHTMLEvent(eventNames().errorEvent, true, false);
    else {
        m_view->frame()->loader()->executeScript(cachedScriptUrl, 1, scriptSource);
        EventTargetNodeCast(e.get())->dispatchHTMLEvent(eventNames().loadEvent, false, false);
    }
    
    m_scriptElement = 0;
    
    if (!m_requestingScript)
        resumeParsing();
}

bool XMLTokenizer::isWaitingForScripts() const
{
    return m_pendingScript;
}

void XMLTokenizer::pauseParsing()
{
    if (m_parsingFragment)
        return;
    
    m_parserPaused = true;
}

}

