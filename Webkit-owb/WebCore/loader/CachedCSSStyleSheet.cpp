/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/

/*
* This file was modified by Electronic Arts Inc Copyright � 2009
*/

#include "config.h"
#include "CachedCSSStyleSheet.h"

#include "Cache.h"
#include "CachedResourceClient.h"
#include "CachedResourceClientWalker.h"
#include "TextResourceDecoder.h"
#include "loader.h"
#include <wtf/Vector.h>

namespace WebCore {

CachedCSSStyleSheet::CachedCSSStyleSheet(const String& url, const String& charset)
    : CachedResource(url, CSSStyleSheet)
    , m_decoder(TextResourceDecoder::create("text/css", charset))
{
    // Prefer text/css but accept any type (dell.com serves a stylesheet
    // as text/html; see <http://bugs.webkit.org/show_bug.cgi?id=11451>).
    setAccept("text/css,*/*;q=0.1");
}

CachedCSSStyleSheet::~CachedCSSStyleSheet()
{
}

void CachedCSSStyleSheet::addClient(CachedResourceClient *c)
{
    CachedResource::addClient(c);

    if (!m_loading)
        c->setCSSStyleSheet(m_url, m_decoder->encoding().name(), this);
}

void CachedCSSStyleSheet::setEncoding(const String& chs)
{
    m_decoder->setEncoding(chs, TextResourceDecoder::EncodingFromHTTPHeader);
}

String CachedCSSStyleSheet::encoding() const
{
    return m_decoder->encoding().name();
}

void CachedCSSStyleSheet::data(PassRefPtr<SharedBuffer> data, bool allDataReceived)
{
    if (!allDataReceived)
        return;

    m_data = data;
    setEncodedSize(m_data.get() ? m_data->size() : 0);
    if (m_data.get()) {
        m_sheet = m_decoder->decode(m_data->data(), encodedSize());
        m_sheet += m_decoder->flush();
    }
    m_loading = false;
    checkNotify();
}

void CachedCSSStyleSheet::checkNotify()
{
    if (m_loading)
        return;

    CachedResourceClientWalker w(m_clients);
    while (CachedResourceClient *c = w.next())
    {
        // Change by Chris Sidhall and Paul Pedriana, March 5, 2009.
        // We were getting cases whereby the response URL was empty, 
        // even though our ResourceHandleManager code seemed to be setting it.
        const String& sURL = m_response.url().string();

        if(sURL.isEmpty())
            c->setCSSStyleSheet(m_url, m_decoder->encoding().name(), this);
        else
            c->setCSSStyleSheet(sURL, m_decoder->encoding().name(), this);
    }

#if USE(LOW_BANDWIDTH_DISPLAY)        
    // if checkNotify() is called from error(), client's setCSSStyleSheet(...)
    // can't find "this" from url, so they can't do clean up if needed.
    // call notifyFinished() to make sure they have a chance.
    CachedResourceClientWalker n(m_clients);
    while (CachedResourceClient* s = n.next())
        s->notifyFinished(this);
#endif        
}

void CachedCSSStyleSheet::error()
{
    m_loading = false;
    m_errorOccurred = true;
    checkNotify();
}

bool CachedCSSStyleSheet::canUseSheet(bool strict) const
{
    if (errorOccurred())
        return false;
        
    if (!strict)
        return true;

    // This check exactly matches Firefox.
    String mimeType = response().mimeType();
    return mimeType.isEmpty() || equalIgnoringCase(mimeType, "text/css") || equalIgnoringCase(mimeType, "application/x-unknown-content-type");
}
 
}
