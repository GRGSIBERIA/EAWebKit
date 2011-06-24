/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
* This file was modified by Electronic Arts Inc Copyright � 2010
*/

#include "config.h"
#include "AccessItem.h"

#include "KURL.h"
#include "PlatformString.h"
#include "SecurityOrigin.h"

#ifndef NDEBUG
#include "CString.h"
#include <stdio.h>
#endif

namespace WebCore {

AccessItem::AccessItem(const String& accessItemString)
    : m_wildcard(false)
    , m_domainWildcard(false)
    , m_portWildcard(false)
    , m_port(0)
#ifndef NDEBUG
    , m_string(accessItemString)
#endif
{
    parseAccessItem(accessItemString);
}

void AccessItem::parseAccessItem(const String& accessItemString)
{
    // Parse the Access Item according to Section 4.1 (Access Item) of the 
    // Access Control for Cross-site Requests spec.
    // W3C Working Draft 14 February 2008

    //   access-item    = [scheme "://"] domain-pattern [":" port-pattern] | "*"
    //   domain-pattern = domain | "*." domain
    //   port-pattern   = port | "*"

    // FIXME: Parse the AccessItem.

    if (accessItemString == "*") {
        m_wildcard = true;
        m_valid = true;
        return;
    }

    // FIXME: Remove this when parseAccessItem is fully implemented according to the
    // specification.
    KURL accessItemURL(accessItemString);
    m_valid = accessItemURL.isValid();
    if (m_valid)
        m_origin = SecurityOrigin::create(accessItemURL);
}

bool AccessItem::matches(const SecurityOrigin* accessControlOrigin) const
{
    if (m_wildcard)
        return true;

    // FIXME: Remove this when parseAccessItem is implemented and instead implement the
    // specified matching alogorithm.
    return m_origin->isSameSchemeHostPort(accessControlOrigin);
}

#ifndef NDEBUG
void AccessItem::show()
{
   OWB_PRINTF_FORMATTED("    AccessItem::show: %s\n", m_string.utf8().data());
}
#endif

} // namespace WebCore
