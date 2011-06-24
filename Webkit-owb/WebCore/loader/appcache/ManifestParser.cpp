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

#include "config.h"
#include "ManifestParser.h"

#if ENABLE(OFFLINE_WEB_APPLICATIONS)

#include "CharacterNames.h"
#include "KURL.h"
#include "TextEncoding.h"

namespace WebCore {

enum Mode { Explicit, Fallback, OnlineWhitelist };
    
bool parseManifest(const KURL& manifestURL, const char* data, int length, Manifest& manifest)
{
    ASSERT(manifest.explicitURLs.isEmpty());
    ASSERT(manifest.onlineWhitelistedURLs.isEmpty());
    ASSERT(manifest.fallbackURLs.isEmpty());
    
    Mode mode = Explicit;
    String s = UTF8Encoding().decode(data, length);
    
    if (s.isEmpty())
        return false;
    
    // Replace nulls with U+FFFD REPLACEMENT CHARACTER
    s.replace(0, replacementCharacter);
    
    // Look for the magic signature
    if (!s.startsWith("CACHE MANIFEST")) {
        // The magic signature was not found.
        return false;
    }
    
    const UChar* end = s.characters() + s.length();    
    const UChar* p = s.characters() + 14; // "CACHE MANIFEST" is 14 characters.
    
    while (p < end) {
        // Skip whitespace
        if (*p == ' ' || *p == '\t') {
            p++;
        } else
            break;
    }
    
    if (p < end && *p != '\n' && *p != '\r') {
        // The magic signature was invalid
        return false;
    }
    
    while (1) {
        // Skip whitespace
        while (p < end && (*p == '\n' || *p == '\r' || *p == ' ' || *p == '\t'))
            p++;
        
        if (p == end)
            break;
        
        const UChar* lineStart = p;
        
        // Find the end of the line
        while (p < end && *p != '\r' && *p != '\n')
            p++;
        
        // Check if we have a comment
        if (*lineStart == '#')
            continue;
        
        // Get rid of trailing whitespace
        const UChar* tmp = p - 1;
        while (tmp > lineStart && (*tmp == ' ' || *tmp == '\t'))
            tmp--;
        
        String line(lineStart, tmp - lineStart + 1);

        if (line == "CACHE:") 
            mode = Explicit;
        else if (line == "FALLBACK:")
            mode = Fallback;
        else if (line == "NETWORK:")
            mode = OnlineWhitelist;
        else if (mode == Explicit || mode == OnlineWhitelist) {
            KURL url(manifestURL, line);
            
            if (!url.isValid())
                continue;

            if (url.hasRef())
                url.setRef(String());
            
            if (!equalIgnoringCase(url.protocol(), manifestURL.protocol()))
                continue;
            
            if (mode == Explicit)
                manifest.explicitURLs.add(url.string());
            else
                manifest.onlineWhitelistedURLs.add(url.string());
            
        } else if (mode == Fallback) {
            const UChar *p = line.characters();
            const UChar *lineEnd = p + line.length();
            
            // Look for whitespace separating the two URLs
            while (p < lineEnd && *p != '\t' && *p != ' ') 
                p++;

            if (p == lineEnd) {
                // There was no whitespace separating the URLs.
                continue;
            }
            
            KURL namespaceURL(manifestURL, String(line.characters(), p - line.characters()));
            if (!namespaceURL.isValid())
                continue;
            
            // Check that the namespace URL has the same scheme/host/port as the manifest URL.
            if (!protocolHostAndPortAreEqual(manifestURL, namespaceURL))
                continue;
                                   
            while (p < lineEnd && (*p == '\t' || *p == ' '))
                p++;

            KURL fallbackURL(String(p, line.length() - (p - line.characters())));

            if (!fallbackURL.isValid())
                continue;
            
            if (!equalIgnoringCase(fallbackURL.protocol(), manifestURL.protocol()))
                continue;
            
            manifest.fallbackURLs.add(namespaceURL, fallbackURL);            
        } else 
            ASSERT_NOT_REACHED();
    }

    return true;
}
    
}

#endif // ENABLE(OFFLINE_WEB_APPLICATIONS)
