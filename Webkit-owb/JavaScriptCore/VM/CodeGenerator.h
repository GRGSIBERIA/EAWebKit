// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Cameron Zwarich <cwzwarich@uwaterloo.ca>
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

#ifndef CodeGenerator_h
#define CodeGenerator_h

#include <wtf/FastAllocBase.h>
#include "CodeBlock.h"
#include "HashTraits.h"
#include "Instruction.h"
#include "LabelID.h"
#include "Machine.h"
#include "RegisterID.h"
#include "SegmentedVector.h"
#include "SymbolTable.h"
#include "debugger.h"
#include "nodes.h"
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>

namespace KJS {

    class Identifier;
    class ScopeChain;
    class ScopeNode;

    // JumpContexts are used to track entry and exit points for javascript loops and switch statements
    struct JumpContext {
        LabelStack* labels;
        LabelID* continueTarget;
        LabelID* breakTarget;
        int scopeDepth;
        bool isValidUnlabeledBreakTarget;
    };

    struct FinallyContext {
        LabelID* finallyAddr;
        RegisterID* retAddrDst;
    };

    struct ControlFlowContext: public WTF::FastAllocBase {
        bool isFinallyBlock;
        FinallyContext finallyContext;
    };

    class CodeGenerator: public WTF::FastAllocBase {
    public:
        typedef DeclarationStacks::VarStack VarStack;
        typedef DeclarationStacks::FunctionStack FunctionStack;

        static void setDumpsGeneratedCode(bool dumpsGeneratedCode);

        CodeGenerator(ProgramNode*, const Debugger*, const ScopeChain&, SymbolTable*, CodeBlock*, VarStack&, FunctionStack&);
        CodeGenerator(FunctionBodyNode*, const Debugger*, const ScopeChain&, SymbolTable*, CodeBlock*);
        CodeGenerator(EvalNode*, const Debugger*, const ScopeChain&, SymbolTable*, EvalCodeBlock*);

        ~CodeGenerator();

        JSGlobalData* globalData() const { return m_globalData; }
        const CommonIdentifiers& propertyNames() const { return *m_globalData->propertyNames; }

        void generate();

        // Returns the register corresponding to a local variable, or 0 if no
        // such register exists. Registers returned by registerForLocal do not
        // require explicit reference counting.
        RegisterID* registerForLocal(const Identifier&);
        // Behaves as registerForLocal does, but ignores dynamic scope as
        // dynamic scope should not interfere with const initialisation
        RegisterID* registerForLocalConstInit(const Identifier&);

        // Searches the scope chain in an attempt to  statically locate the requested
        // property.  Returns false if for any reason the property cannot be safely
        // optimised at all.  Otherwise it will return the index and depth of the
        // VariableObject that defines the property.  If the property cannot be found
        // statically, depth will contain the depth of the scope chain where dynamic
        // lookup must begin.
        //
        // NB: depth does _not_ include the local scope.  eg. a depth of 0 refers
        // to the scope containing this codeblock.
        bool findScopedProperty(const Identifier&, int& index, size_t& depth);

        // Returns the register storing "this"
        RegisterID* thisRegister() { return &m_thisRegister; }

        bool isLocal(const Identifier&);
        bool isLocalConstant(const Identifier&);

        // Returns the next available temporary register. Registers returned by
        // newTemporary require a modified form of reference counting: any
        // register with a refcount of 0 is considered "available", meaning that
        // the next instruction may overwrite it.
        RegisterID* newTemporary();

        RegisterID* highestUsedRegister();

        // The same as newTemporary(), but this function returns "suggestion" if
        // "suggestion" is a temporary. This function is helpful in situations
        // where you've put "suggestion" in a RefPtr, but you'd like to allow
        // the next instruction to overwrite it anyway.
        RegisterID* newTemporaryOr(RegisterID* suggestion) { return suggestion->isTemporary() ? suggestion : newTemporary(); }

        // Functions for handling of dst register

        // Returns a place to write intermediate values of an operation
        // which reuses dst if it is safe to do so.
        RegisterID* tempDestination(RegisterID* dst)
        {
            return (dst && dst != ignoredResult() && dst->isTemporary()) ? dst : newTemporary();
        }

        // Returns the place to write the final output of an operation.
        RegisterID* finalDestination(RegisterID* originalDst, RegisterID* tempDst = 0)
        {
            if (originalDst && originalDst != ignoredResult())
                return originalDst;
            ASSERT(tempDst != ignoredResult());
            if (tempDst && tempDst->isTemporary())
                return tempDst;
            return newTemporary();
        }

        RegisterID* destinationForAssignResult(RegisterID* dst)
        {
            if (dst && dst != ignoredResult() && m_codeBlock->needsFullScopeChain)
                return dst->isTemporary() ? dst : newTemporary();
            return 0;
        }

        // Moves src to dst if dst is not null and is different from src, otherwise just returns src.
        RegisterID* moveToDestinationIfNeeded(RegisterID* dst, RegisterID* src)
        {
            return dst == ignoredResult() ? 0 : (dst && dst != src) ? emitMove(dst, src) : src;
        }

        PassRefPtr<LabelID> newLabel();

        // The emitNode functions are just syntactic sugar for calling
        // Node::emitCode. These functions accept a 0 for the register,
        // meaning that the node should allocate a register, or ignoredResult(),
        // meaning that the node need not put the result in a register.
        // Other emit functions do not accept 0 or ignoredResult().
        RegisterID* emitNode(RegisterID* dst, Node* n)
        {
            // Node::emitCode assumes that dst, if provided, is either a local or a referenced temporary.
            ASSERT(!dst || dst == ignoredResult() || !dst->isTemporary() || dst->refCount());
            if (!m_codeBlock->lineInfo.size() || m_codeBlock->lineInfo.last().lineNumber != n->lineNo()) {
                LineInfo info = { instructions().size(), n->lineNo() };
                m_codeBlock->lineInfo.append(info);
            }
            return n->emitCode(*this, dst);
        }

        RegisterID* emitNode(Node* n)
        {
            return emitNode(0, n);
        }

        ALWAYS_INLINE bool leftHandSideNeedsCopy(bool rightHasAssignments, bool rightIsPure)
        {
            return (m_codeType != FunctionCode || m_codeBlock->needsFullScopeChain || rightHasAssignments) && !rightIsPure;
        }

        ALWAYS_INLINE PassRefPtr<RegisterID> emitNodeForLeftHandSide(ExpressionNode* n, bool rightHasAssignments, bool rightIsPure)
        {
            if (leftHandSideNeedsCopy(rightHasAssignments, rightIsPure)) {
                PassRefPtr<RegisterID> dst = newTemporary();
                emitNode(dst.get(), n);
                return dst;
            }

            return PassRefPtr<RegisterID>(emitNode(n));
        }

        RegisterID* emitLoad(RegisterID* dst, bool);
        RegisterID* emitLoad(RegisterID* dst, double);
        RegisterID* emitLoad(RegisterID* dst, JSValue*);

        RegisterID* emitNullaryOp(OpcodeID, RegisterID* dst);
        RegisterID* emitUnaryOp(OpcodeID, RegisterID* dst, RegisterID* src);
        RegisterID* emitBinaryOp(OpcodeID, RegisterID* dst, RegisterID* src1, RegisterID* src2);
        RegisterID* emitUnaryNoDstOp(OpcodeID, RegisterID* src);

        RegisterID* emitNewObject(RegisterID* dst) { return emitNullaryOp(op_new_object, dst); }
        RegisterID* emitNewArray(RegisterID* dst, ElementNode*); // stops at first elision

        RegisterID* emitNewFunction(RegisterID* dst, FuncDeclNode* func);
        RegisterID* emitNewFunctionExpression(RegisterID* dst, FuncExprNode* func);
        RegisterID* emitNewRegExp(RegisterID* dst, RegExp* regExp);

        RegisterID* emitMove(RegisterID* dst, RegisterID* src);

        RegisterID* emitToJSNumber(RegisterID* dst, RegisterID* src) { return emitUnaryOp(op_to_jsnumber, dst, src); }
        RegisterID* emitPreInc(RegisterID* srcDst);
        RegisterID* emitPreDec(RegisterID* srcDst);
        RegisterID* emitPostInc(RegisterID* dst, RegisterID* srcDst);
        RegisterID* emitPostDec(RegisterID* dst, RegisterID* srcDst);

        RegisterID* emitInstanceOf(RegisterID* dst, RegisterID* value, RegisterID* base) { return emitBinaryOp(op_instanceof, dst, value, base); }
        RegisterID* emitTypeOf(RegisterID* dst, RegisterID* src) { return emitUnaryOp(op_typeof, dst, src); }
        RegisterID* emitIn(RegisterID* dst, RegisterID* property, RegisterID* base) { return emitBinaryOp(op_in, dst, property, base); }

        RegisterID* emitResolve(RegisterID* dst, const Identifier& property);
        RegisterID* emitGetScopedVar(RegisterID* dst, size_t skip, int index);
        RegisterID* emitPutScopedVar(size_t skip, int index, RegisterID* value);

        RegisterID* emitResolveBase(RegisterID* dst, const Identifier& property);
        RegisterID* emitResolveWithBase(RegisterID* baseDst, RegisterID* propDst, const Identifier& property);
        RegisterID* emitResolveFunction(RegisterID* baseDst, RegisterID* funcDst, const Identifier& property);

        RegisterID* emitGetById(RegisterID* dst, RegisterID* base, const Identifier& property);
        RegisterID* emitPutById(RegisterID* base, const Identifier& property, RegisterID* value);
        RegisterID* emitDeleteById(RegisterID* dst, RegisterID* base, const Identifier&);
        RegisterID* emitGetByVal(RegisterID* dst, RegisterID* base, RegisterID* property);
        RegisterID* emitPutByVal(RegisterID* base, RegisterID* property, RegisterID* value);
        RegisterID* emitDeleteByVal(RegisterID* dst, RegisterID* base, RegisterID* property);
        RegisterID* emitPutByIndex(RegisterID* base, unsigned index, RegisterID* value);
        RegisterID* emitPutGetter(RegisterID* base, const Identifier& property, RegisterID* value);
        RegisterID* emitPutSetter(RegisterID* base, const Identifier& property, RegisterID* value);

        RegisterID* emitCall(RegisterID* dst, RegisterID* func, RegisterID* base, ArgumentsNode*);
        RegisterID* emitCallEval(RegisterID* dst, RegisterID* func, RegisterID* base, ArgumentsNode*);

        RegisterID* emitReturn(RegisterID* src) { return emitUnaryNoDstOp(op_ret, src); } 
        RegisterID* emitEnd(RegisterID* src) { return emitUnaryNoDstOp(op_end, src); }

        RegisterID* emitConstruct(RegisterID* dst, RegisterID* func, ArgumentsNode*);

        PassRefPtr<LabelID> emitLabel(LabelID*);
        PassRefPtr<LabelID> emitJump(LabelID* target);
        PassRefPtr<LabelID> emitJumpIfTrue(RegisterID* cond, LabelID* target);
        PassRefPtr<LabelID> emitJumpIfFalse(RegisterID* cond, LabelID* target);
        PassRefPtr<LabelID> emitJumpScopes(LabelID* target, int targetScopeDepth);

        PassRefPtr<LabelID> emitJumpSubroutine(RegisterID* retAddrDst, LabelID*);
        void emitSubroutineReturn(RegisterID* retAddrSrc);

        RegisterID* emitGetPropertyNames(RegisterID* dst, RegisterID* base) { return emitUnaryOp(op_get_pnames, dst, base); }
        RegisterID* emitNextPropertyName(RegisterID* dst, RegisterID* iter, LabelID* target);

        RegisterID* emitCatch(RegisterID*, LabelID* start, LabelID* end);
        void emitThrow(RegisterID* exc) { emitUnaryNoDstOp(op_throw, exc); }
        RegisterID* emitNewError(RegisterID* dst, ErrorType type, JSValue* message);

        RegisterID* emitPushScope(RegisterID* scope);
        void emitPopScope();

        void emitDebugHook(DebugHookID, int firstLine, int lastLine);

        int scopeDepth() { return m_dynamicScopeDepth + m_finallyDepth; }

        void pushFinallyContext(LabelID* target, RegisterID* returnAddrDst);
        void popFinallyContext();
        bool inContinueContext() { return m_continueDepth > 0; };
        bool inJumpContext() { return m_jumpContextStack.size() > 0; };
        void pushJumpContext(LabelStack*, LabelID* continueTarget, LabelID* breakTarget, bool isValidUnlabeledBreakTarget);
        void popJumpContext();
        JumpContext* jumpContextForContinue(const Identifier&);
        JumpContext* jumpContextForBreak(const Identifier&);

        CodeType codeType() const { return m_codeType; }

        ExecState* globalExec() { return m_scopeChain->globalObject()->globalExec(); }

    private:
        void emitOpcode(OpcodeID);
        void retrieveLastBinaryOp(int& dstIndex, int& src1Index, int& src2Index);
        void rewindBinaryOp();

        PassRefPtr<LabelID> emitComplexJumpScopes(LabelID* target, ControlFlowContext* topScope, ControlFlowContext* bottomScope);
        struct JSValueHashTraits : HashTraits<JSValue*> {
            static void constructDeletedValue(JSValue** slot) { *slot = JSImmediate::impossibleValue(); }
            static bool isDeletedValue(JSValue* value) { return value == JSImmediate::impossibleValue(); }
        };

        typedef HashMap<JSValue*, unsigned, DefaultHash<JSValue*>::Hash, JSValueHashTraits> JSValueMap;

        struct IdentifierMapIndexHashTraits: public WTF::FastAllocBase {
            typedef int TraitType;
            typedef IdentifierMapIndexHashTraits StorageTraits;
            static int emptyValue() { return std::numeric_limits<int>::max(); }
            static const bool emptyValueIsZero = false;
            static const bool needsDestruction = false;
            static const bool needsRef = false;
        };

        typedef HashMap<RefPtr<UString::Rep>, int, IdentifierRepHash, HashTraits<RefPtr<UString::Rep> >, IdentifierMapIndexHashTraits> IdentifierMap;

        RegisterID* emitCall(OpcodeID, RegisterID*, RegisterID*, RegisterID*, ArgumentsNode*);

        // Maps a register index in the symbol table to a RegisterID index in m_locals.
        int localsIndex(int registerIndex) { return -registerIndex - 1; }

        // Returns the RegisterID corresponding to ident.
        RegisterID* addVar(const Identifier& ident, bool isConstant)
        {
            RegisterID* local;
            addVar(ident, isConstant, local);
            return local;
        }
        // Returns true if a new RegisterID was added, false if a pre-existing RegisterID was re-used.
        bool addVar(const Identifier&, bool isConstant, RegisterID*&);


        // Returns the RegisterID corresponding to ident.
        RegisterID* addGlobalVar(const Identifier& ident, bool isConstant)
        {
            RegisterID* local;
            addGlobalVar(ident, isConstant, local);
            return local;
        }
        // Returns true if a new RegisterID was added, false if a pre-existing RegisterID was re-used.
        bool addGlobalVar(const Identifier&, bool isConstant, RegisterID*&);

        RegisterID* addParameter(const Identifier&);

        unsigned addConstant(FuncDeclNode*);
        unsigned addConstant(FuncExprNode*);
        unsigned addConstant(const Identifier&);
        unsigned addConstant(JSValue*);
        unsigned addRegExp(RegExp* r);

        Vector<Instruction>& instructions() { return m_codeBlock->instructions; }
        SymbolTable& symbolTable() { return *m_symbolTable; }
        Vector<HandlerInfo>& exceptionHandlers() { return m_codeBlock->exceptionHandlers; }

        bool shouldOptimizeLocals() { return (m_codeType != EvalCode) && !m_dynamicScopeDepth; }
        bool canOptimizeNonLocals() { return (m_codeType == FunctionCode) && !m_dynamicScopeDepth && !m_codeBlock->usesEval; }

        bool m_shouldEmitDebugHooks;

        const ScopeChain* m_scopeChain;
        SymbolTable* m_symbolTable;

        ScopeNode* m_scopeNode;
        CodeBlock* m_codeBlock;

        HashSet<RefPtr<UString::Rep>, IdentifierRepHash> m_functions;
        RegisterID m_thisRegister;
        SegmentedVector<RegisterID, 512> m_locals;
        SegmentedVector<RegisterID, 512> m_temporaries;
        SegmentedVector<LabelID, 512> m_labels;
        int m_finallyDepth;
        int m_dynamicScopeDepth;
        CodeType m_codeType;

        Vector<JumpContext> m_jumpContextStack;
        int m_continueDepth;
        Vector<ControlFlowContext> m_scopeContextStack;

        int m_nextVar;
        int m_nextParameter;

        int m_globalVarStorageOffset;

        // Constant pool
        IdentifierMap m_identifierMap;
        JSValueMap m_jsValueMap;

        JSGlobalData* m_globalData;

        OpcodeID m_lastOpcodeID;

#ifndef NDEBUG
        static bool s_dumpsGeneratedCode;
#endif
    };

}

#endif // CodeGenerator_h
