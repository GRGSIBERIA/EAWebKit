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
#include "AccessControlList.h"

#include "AccessItemRule.h"
#include "PlatformString.h"
#include "SecurityOrigin.h"

#ifndef NDEBUG
#include <stdio.h>
#endif

namespace WebCore {

AccessControlList::AccessControlList(const String& accessControlHeader)
{
    parseAccessControlHeader(accessControlHeader);
}

AccessControlList::~AccessControlList()
{
    deleteAllValues(m_list);
}

void AccessControlList::parseAccessControlHeader(const String& accessControlHeader)
{
    Vector<String> rules;
    accessControlHeader.split(',', rules);
    for (size_t i = 0; i < rules.size(); ++i)
        m_list.append(new AccessItemRule(rules[i]));
}

bool AccessControlList::checkOrigin(const SecurityOrigin* accessControlOrigin) const
{
    for (size_t i = 0; i < m_list.size(); ++i) {
        AccessItemRule* rule = m_list[i];
        if (!rule->allowListMatchesAny(accessControlOrigin))
            continue;
        if (rule->excludeListMatchesAny(accessControlOrigin))
            continue;
        return true;
    }
    return false;
}

#ifndef NDEBUG
void AccessControlList::show()
{
   OWB_PRINTF_FORMATTED("AccessControlList::show count: %d\n", static_cast<int>(m_list.size()));
    for (size_t i = 0; i < m_list.size(); ++i)
        m_list[i]->show();
   OWB_PRINTF("\n\n");
}
#endif

} // namespace WebCore
