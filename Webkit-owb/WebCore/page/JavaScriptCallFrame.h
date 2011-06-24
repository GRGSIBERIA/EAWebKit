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
* This file was modified by Electronic Arts Inc Copyright � 2009
*/

#ifndef JavaScriptCallFrame_h
#define JavaScriptCallFrame_h

#include <kjs/ExecState.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <kjs/DebuggerCallFrame.h>

namespace WebCore {

    class String;

    class JavaScriptCallFrame : public RefCounted<JavaScriptCallFrame> {
    public:
        static PassRefPtr<JavaScriptCallFrame> create(const KJS::DebuggerCallFrame& debuggerCallFrame, PassRefPtr<JavaScriptCallFrame> caller, int sourceID, int line)
        {
            return adoptRef(new JavaScriptCallFrame(debuggerCallFrame, caller, sourceID, line));
        }

        void invalidate() { m_isValid = false; }
        bool isValid() const { return m_isValid; }

        JavaScriptCallFrame* caller();

        int sourceIdentifier() const { return m_sourceID; }
        int line() const { return m_line; }
        void update(const KJS::DebuggerCallFrame& debuggerCallFrame, int sourceID, int line)
        {
            m_debuggerCallFrame = debuggerCallFrame;
            m_line = line;
            m_sourceID = sourceID;
        }

        String functionName() const;
        KJS::DebuggerCallFrame::Type type() const;
        const KJS::ScopeChainNode* scopeChain() const;

        KJS::JSObject* thisObject() const;
        KJS::JSValue* evaluate(const KJS::UString& script, KJS::JSValue*& exception) const;
        
    private:
        JavaScriptCallFrame(const KJS::DebuggerCallFrame&, PassRefPtr<JavaScriptCallFrame> caller, int sourceID, int line);

        KJS::DebuggerCallFrame m_debuggerCallFrame;
        RefPtr<JavaScriptCallFrame> m_caller;
        int m_sourceID;
        int m_line;
        bool m_isValid;
    };

} // namespace WebCore

#endif // JavaScriptCallFrame_h
