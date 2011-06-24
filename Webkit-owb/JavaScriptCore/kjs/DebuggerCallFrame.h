/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
 
/*
* This file was modified by Electronic Arts Inc Copyright � 2009
*/

#ifndef DebuggerCallFrame_h
#define DebuggerCallFrame_h

#include <wtf/FastAllocBase.h>
namespace KJS {
    
    class CodeBlock;
    class JSGlobalObject;
    class JSObject;
    class JSValue;
    class Machine;
    class UString;
    class Register;
    class ScopeChainNode;
    
    class DebuggerCallFrame {
public:
// Placement operator new.
void* operator new(size_t, void* p) { return p; }
void* operator new[](size_t, void* p) { return p; }
 
void* operator new(size_t size)
{
     void* p = fastMalloc(size);
     fastMallocMatchValidateMalloc(p, WTF::Internal::AllocTypeClassNew);
     return p;
}
 
void operator delete(void* p)
{
     fastMallocMatchValidateFree(p, WTF::Internal::AllocTypeClassNew);
     fastFree(p);  // We don't need to check for a null pointer; the compiler does this.
}
 
void* operator new[](size_t size)
{
     void* p = fastMalloc(size);
     fastMallocMatchValidateMalloc(p, WTF::Internal::AllocTypeClassNewArray);
     return p;
}
 
void operator delete[](void* p)
{
     fastMallocMatchValidateFree(p, WTF::Internal::AllocTypeClassNewArray);
     fastFree(p);  // We don't need to check for a null pointer; the compiler does this.
}
    public:
        typedef enum {
            ProgramType,
            FunctionType
        } Type;

        DebuggerCallFrame(JSGlobalObject* dynamicGlobalObject, const CodeBlock* codeBlock, ScopeChainNode* scopeChain, JSValue* exception, Register** registerBase, int registerOffset)
            : m_dynamicGlobalObject(dynamicGlobalObject)
            , m_codeBlock(codeBlock)
            , m_scopeChain(scopeChain)
            , m_exception(exception)
            , m_registerBase(registerBase)
            , m_registerOffset(registerOffset)
        {
        }

        JSGlobalObject* dynamicGlobalObject() const { return m_dynamicGlobalObject; }
        const ScopeChainNode* scopeChain() const { return m_scopeChain; }
        const UString* functionName() const;
        DebuggerCallFrame::Type type() const;
        JSObject* thisObject() const;
        JSValue* evaluate(const UString&, JSValue*& exception) const;
        JSValue* exception() const { return m_exception; }

    private:
        Register* r() const;
        Register* callFrame() const;

        JSGlobalObject* m_dynamicGlobalObject;
        const CodeBlock* m_codeBlock;
        ScopeChainNode* m_scopeChain;
        JSValue* m_exception;
        Register** m_registerBase;
        int m_registerOffset;
    };

} // namespace KJS

#endif // DebuggerCallFrame_h
