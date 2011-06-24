/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Cameron Zwarich (cwzwarich@uwaterloo.ca)
 *  Copyright (C) 2007 Maks Orlovich
 *  Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

/*
* This file was modified by Electronic Arts Inc Copyright � 2009
*/

#ifndef NODES_H_
#define NODES_H_

#include <wtf/FastAllocBase.h>
#include "JSString.h"
#include "LabelStack.h"
#include "Opcode.h"
#include "RegisterID.h"
#include "SourceRange.h"
#include "SymbolTable.h"
#include "regexp.h"
#include <wtf/ListRefPtr.h>
#include <wtf/MathExtras.h>
#include <wtf/OwnPtr.h>
#include <wtf/UnusedParam.h>
#include <wtf/Vector.h>

#if PLATFORM(X86) && COMPILER(GCC)
#define KJS_FAST_CALL __attribute__((regparm(3)))
#else
#define KJS_FAST_CALL
#endif

namespace KJS {

    class CodeBlock;
    class CodeGenerator;
    class FuncDeclNode;
    class Node;
    class EvalCodeBlock;
    class ProgramCodeBlock;
    class PropertyListNode;
    class SourceStream;

    enum Operator {
        OpEqual,
        OpPlusEq,
        OpMinusEq,
        OpMultEq,
        OpDivEq,
        OpPlusPlus,
        OpMinusMinus,
        OpAndEq,
        OpXOrEq,
        OpOrEq,
        OpModEq,
        OpLShift,
        OpRShift,
        OpURShift,
    };

    enum Precedence {
        PrecPrimary,
        PrecMember,
        PrecCall,
        PrecLeftHandSide,
        PrecPostfix,
        PrecUnary,
        PrecMultiplicative,
        PrecAdditive,
        PrecShift,
        PrecRelational,
        PrecEquality,
        PrecBitwiseAnd,
        PrecBitwiseXor,
        PrecBitwiseOr,
        PrecLogicalAnd,
        PrecLogicalOr,
        PrecConditional,
        PrecAssignment,
        PrecExpression
    };

    struct DeclarationStacks {
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
        typedef Vector<Node*, 16> NodeStack;
        enum { IsConstant = 1, HasInitializer = 2 } VarAttrs;
        typedef Vector<std::pair<Identifier, unsigned>, 16> VarStack;
        typedef Vector<RefPtr<FuncDeclNode>, 16> FunctionStack;

        DeclarationStacks(ExecState* e, NodeStack& n, VarStack& v, FunctionStack& f)
            : exec(e)
            , nodeStack(n)
            , varStack(v)
            , functionStack(f)
        {
        }

        ExecState* exec;
        NodeStack& nodeStack;
        VarStack& varStack;
        FunctionStack& functionStack;
    };

    class ParserRefCounted : Noncopyable {
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
    protected:
        ParserRefCounted(JSGlobalData*) KJS_FAST_CALL;

        JSGlobalData* m_globalData;

    public:
        void ref() KJS_FAST_CALL;
        void deref() KJS_FAST_CALL;
        bool hasOneRef() KJS_FAST_CALL;

        static void deleteNewObjects(JSGlobalData*) KJS_FAST_CALL;

        virtual ~ParserRefCounted();
    };

    class Node : public ParserRefCounted {
    public:
        typedef DeclarationStacks::NodeStack NodeStack;
        typedef DeclarationStacks::VarStack VarStack;
        typedef DeclarationStacks::FunctionStack FunctionStack;

        Node(JSGlobalData*) KJS_FAST_CALL;

        /*
            Return value: The register holding the production's value.
                     dst: An optional parameter specifying the most efficient
                          destination at which to store the production's value.
                          The callee must honor dst.

            dst provides for a crude form of copy propagation. For example,

            x = 1

            becomes
            
            load r[x], 1
            
            instead of 

            load r0, 1
            mov r[x], r0
            
            because the assignment node, "x =", passes r[x] as dst to the number
            node, "1".
        */
        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* dst = 0) KJS_FAST_CALL 
        {
            ASSERT_WITH_MESSAGE(0, "Don't know how to generate code for:\n%s\n", toString().ascii());
            UNUSED_PARAM(dst); 
            return 0; 
        } // FIXME: Make this pure virtual.

        UString toString() const KJS_FAST_CALL;
        int lineNo() const KJS_FAST_CALL { return m_line; }

        virtual bool isReturnNode() const KJS_FAST_CALL { return false; }

        // Serialization.
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL = 0;
        virtual Precedence precedence() const = 0;
        virtual bool needsParensIfLeftmost() const { return false; }
        
    protected:
        Node(JSGlobalData*, JSType) KJS_FAST_CALL; // used by ExpressionNode

        RegisterID* emitThrowError(CodeGenerator&, ErrorType, const char* msg);
        RegisterID* emitThrowError(CodeGenerator&, ErrorType, const char* msg, const Identifier&);

        int m_line : 28;
        unsigned m_expectedReturnType : 3; // JSType
    };

    class ExpressionNode : public Node {
    public:
        ExpressionNode(JSGlobalData* globalData) KJS_FAST_CALL : Node(globalData) {}
        ExpressionNode(JSGlobalData* globalData, JSType expectedReturn) KJS_FAST_CALL
            : Node(globalData, expectedReturn)
        {
        }

        virtual bool isNumber() const KJS_FAST_CALL { return false; }
        virtual bool isPure(CodeGenerator&) const KJS_FAST_CALL { return false; }        
        virtual bool isLocation() const KJS_FAST_CALL { return false; }
        virtual bool isResolveNode() const KJS_FAST_CALL { return false; }
        virtual bool isBracketAccessorNode() const KJS_FAST_CALL { return false; }
        virtual bool isDotAccessorNode() const KJS_FAST_CALL { return false; }

        JSType expectedReturnType() const KJS_FAST_CALL { return static_cast<JSType>(m_expectedReturnType); }

        // This needs to be in public in order to compile using GCC 3.x 
        typedef enum { EvalOperator, FunctionCall } CallerType;
    };

    class StatementNode : public Node {
    public:
        StatementNode(JSGlobalData*) KJS_FAST_CALL;
        void setLoc(int line0, int line1) KJS_FAST_CALL;
        int firstLine() const KJS_FAST_CALL { return lineNo(); }
        int lastLine() const KJS_FAST_CALL { return m_lastLine; }

        virtual void pushLabel(const Identifier& ident) KJS_FAST_CALL { m_labelStack.push(ident); }
        virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }
        virtual bool isEmptyStatement() const KJS_FAST_CALL { return false; }

    protected:
        LabelStack m_labelStack;

    private:
        int m_lastLine;
    };

    class NullNode : public ExpressionNode {
    public:
        NullNode(JSGlobalData* globalData) KJS_FAST_CALL
            : ExpressionNode(globalData, NullType)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecPrimary; }
    };

    class BooleanNode : public ExpressionNode {
    public:
        BooleanNode(JSGlobalData* globalData, bool value) KJS_FAST_CALL
            : ExpressionNode(globalData, BooleanType)
            , m_value(value)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual bool isPure(CodeGenerator&) const KJS_FAST_CALL { return true; }
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecPrimary; }

    protected:
        bool m_value;
    };

    class NumberNode : public ExpressionNode {
    public:
        NumberNode(JSGlobalData* globalData, double v) KJS_FAST_CALL
            : ExpressionNode(globalData, NumberType)
            , m_double(v)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return signbit(m_double) ? PrecUnary : PrecPrimary; }

        virtual bool isNumber() const KJS_FAST_CALL { return true; }
        virtual bool isPure(CodeGenerator&) const KJS_FAST_CALL { return true; }
        double value() const KJS_FAST_CALL { return m_double; }
        virtual void setValue(double d) KJS_FAST_CALL { m_double = d; }

    protected:
        double m_double;
    };

    class ImmediateNumberNode : public NumberNode {
    public:
        ImmediateNumberNode(JSGlobalData* globalData, JSValue* v, double d) KJS_FAST_CALL
            : NumberNode(globalData, d)
            , m_value(v)
        {
            ASSERT(v == JSImmediate::from(d));
        }

        virtual void setValue(double d) KJS_FAST_CALL { m_double = d; m_value = JSImmediate::from(d); ASSERT(m_value); }

    private:
        JSValue* m_value; // This is never a JSCell, only JSImmediate, thus no ProtectedPtr
    };

    class StringNode : public ExpressionNode {
    public:
        StringNode(JSGlobalData* globalData, const UString* v) KJS_FAST_CALL
            : ExpressionNode(globalData, StringType)
            , m_value(*v)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual bool isPure(CodeGenerator&) const KJS_FAST_CALL { return true; }
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecPrimary; }

    private:
        UString m_value;
    };

    class RegExpNode : public ExpressionNode {
    public:
        RegExpNode(JSGlobalData* globalData, const UString& pattern, const UString& flags) KJS_FAST_CALL
            : ExpressionNode(globalData)
            , m_regExp(RegExp::create(pattern, flags))
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecPrimary; }

    private:
        RefPtr<RegExp> m_regExp;
    };

    class ThisNode : public ExpressionNode {
    public:
        ThisNode(JSGlobalData* globalData) KJS_FAST_CALL
            : ExpressionNode(globalData)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecPrimary; }
    };

    class ResolveNode : public ExpressionNode {
    public:
        ResolveNode(JSGlobalData* globalData, const Identifier& ident) KJS_FAST_CALL
            : ExpressionNode(globalData)
            , m_ident(ident)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecPrimary; }

        virtual bool isPure(CodeGenerator&) const KJS_FAST_CALL;
        virtual bool isLocation() const KJS_FAST_CALL { return true; }
        virtual bool isResolveNode() const KJS_FAST_CALL { return true; }
        const Identifier& identifier() const KJS_FAST_CALL { return m_ident; }

    protected:
        Identifier m_ident;
        int m_index; // Used by LocalVarAccessNode and ScopedVarAccessNode.
        size_t m_scopeDepth; // Used by ScopedVarAccessNode
    };

    class ElementNode : public Node {
    public:
        ElementNode(JSGlobalData* globalData, int elision, ExpressionNode* node) KJS_FAST_CALL
            : Node(globalData)
            , m_elision(elision)
            , m_node(node)
        {
        }

        ElementNode(JSGlobalData* globalData, ElementNode* l, int elision, ExpressionNode* node) KJS_FAST_CALL
            : Node(globalData)
            , m_elision(elision)
            , m_node(node)
        {
            l->m_next = this;
        }

        virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;

        int elision() const { return m_elision; }
        ExpressionNode* value() { return m_node.get(); }

        ElementNode* next() { return m_next.get(); }
        PassRefPtr<ElementNode> releaseNext() KJS_FAST_CALL { return m_next.release(); }

    private:
        ListRefPtr<ElementNode> m_next;
        int m_elision;
        RefPtr<ExpressionNode> m_node;
    };

    class ArrayNode : public ExpressionNode {
    public:
        ArrayNode(JSGlobalData* globalData, int elision) KJS_FAST_CALL
            : ExpressionNode(globalData)
            , m_elision(elision)
            , m_optional(true)
        {
        }

        ArrayNode(JSGlobalData* globalData, ElementNode* element) KJS_FAST_CALL
            : ExpressionNode(globalData)
            , m_element(element)
            , m_elision(0)
            , m_optional(false)
        {
        }

        ArrayNode(JSGlobalData* globalData, int elision, ElementNode* element) KJS_FAST_CALL
            : ExpressionNode(globalData)
            , m_element(element)
            , m_elision(elision)
            , m_optional(true)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecPrimary; }

    private:
        RefPtr<ElementNode> m_element;
        int m_elision;
        bool m_optional;
    };

    class PropertyNode : public Node {
    public:
        enum Type { Constant, Getter, Setter };

        PropertyNode(JSGlobalData* globalData, const Identifier& name, ExpressionNode* assign, Type type) KJS_FAST_CALL
            : Node(globalData)
            , m_name(name)
            , m_assign(assign)
            , m_type(type)
        {
        }

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }

        const Identifier& name() const { return m_name; }

    private:
        friend class PropertyListNode;
        Identifier m_name;
        RefPtr<ExpressionNode> m_assign;
        Type m_type;
    };

    class PropertyListNode : public Node {
    public:
        PropertyListNode(JSGlobalData* globalData, PropertyNode* node) KJS_FAST_CALL
            : Node(globalData)
            , m_node(node)
        {
        }

        PropertyListNode(JSGlobalData* globalData, PropertyNode* node, PropertyListNode* list) KJS_FAST_CALL
            : Node(globalData)
            , m_node(node)
        {
            list->m_next = this;
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }

        PassRefPtr<PropertyListNode> releaseNext() KJS_FAST_CALL { return m_next.release(); }

    private:
        friend class ObjectLiteralNode;
        RefPtr<PropertyNode> m_node;
        ListRefPtr<PropertyListNode> m_next;
    };

    class ObjectLiteralNode : public ExpressionNode {
    public:
        ObjectLiteralNode(JSGlobalData* globalData) KJS_FAST_CALL
            : ExpressionNode(globalData)
        {
        }

        ObjectLiteralNode(JSGlobalData* globalData, PropertyListNode* list) KJS_FAST_CALL
            : ExpressionNode(globalData)
            , m_list(list)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecPrimary; }
        virtual bool needsParensIfLeftmost() const { return true; }

    private:
        RefPtr<PropertyListNode> m_list;
    };

    class BracketAccessorNode : public ExpressionNode {
    public:
        BracketAccessorNode(JSGlobalData* globalData, ExpressionNode* base, ExpressionNode* subscript, bool subscriptHasAssignments) KJS_FAST_CALL
            : ExpressionNode(globalData)
            , m_base(base)
            , m_subscript(subscript)
            , m_subscriptHasAssignments(subscriptHasAssignments)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecMember; }

        virtual bool isLocation() const KJS_FAST_CALL { return true; }
        virtual bool isBracketAccessorNode() const KJS_FAST_CALL { return true; }
        ExpressionNode* base() KJS_FAST_CALL { return m_base.get(); }
        ExpressionNode* subscript() KJS_FAST_CALL { return m_subscript.get(); }

    private:
        RefPtr<ExpressionNode> m_base;
        RefPtr<ExpressionNode> m_subscript;
        bool m_subscriptHasAssignments;
    };

    class DotAccessorNode : public ExpressionNode {
    public:
        DotAccessorNode(JSGlobalData* globalData, ExpressionNode* base, const Identifier& ident) KJS_FAST_CALL
            : ExpressionNode(globalData)
            , m_base(base)
            , m_ident(ident)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecMember; }

        virtual bool isLocation() const KJS_FAST_CALL { return true; }
        virtual bool isDotAccessorNode() const KJS_FAST_CALL { return true; }
        ExpressionNode* base() const KJS_FAST_CALL { return m_base.get(); }
        const Identifier& identifier() const KJS_FAST_CALL { return m_ident; }

    private:
        RefPtr<ExpressionNode> m_base;
        Identifier m_ident;
    };

    class ArgumentListNode : public Node {
    public:
        ArgumentListNode(JSGlobalData* globalData, ExpressionNode* expr) KJS_FAST_CALL
            : Node(globalData)
            , m_expr(expr)
        {
        }

        ArgumentListNode(JSGlobalData* globalData, ArgumentListNode* listNode, ExpressionNode* expr) KJS_FAST_CALL
            : Node(globalData)
            , m_expr(expr)
        {
            listNode->m_next = this;
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }

        PassRefPtr<ArgumentListNode> releaseNext() KJS_FAST_CALL { return m_next.release(); }

        ListRefPtr<ArgumentListNode> m_next;
        RefPtr<ExpressionNode> m_expr;
    };

    class ArgumentsNode : public Node {
    public:
        ArgumentsNode(JSGlobalData* globalData) KJS_FAST_CALL
            : Node(globalData)
        {
        }

        ArgumentsNode(JSGlobalData* globalData, ArgumentListNode* listNode) KJS_FAST_CALL
            : Node(globalData)
            , m_listNode(listNode)
        {
        }

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }

        RefPtr<ArgumentListNode> m_listNode;
    };

    class NewExprNode : public ExpressionNode {
    public:
        NewExprNode(JSGlobalData* globalData, ExpressionNode* expr) KJS_FAST_CALL
            : ExpressionNode(globalData)
            , m_expr(expr)
        {
        }

        NewExprNode(JSGlobalData* globalData, ExpressionNode* expr, ArgumentsNode* args) KJS_FAST_CALL
            : ExpressionNode(globalData)
            , m_expr(expr)
            , m_args(args)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecLeftHandSide; }

    private:
        RefPtr<ExpressionNode> m_expr;
        RefPtr<ArgumentsNode> m_args;
    };

    class EvalFunctionCallNode : public ExpressionNode {
    public:
        EvalFunctionCallNode(JSGlobalData* globalData, ArgumentsNode* args) KJS_FAST_CALL
            : ExpressionNode(globalData)
            , m_args(args)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecCall; }

    private:
        RefPtr<ArgumentsNode> m_args;
    };

    class FunctionCallValueNode : public ExpressionNode {
    public:
        FunctionCallValueNode(JSGlobalData* globalData, ExpressionNode* expr, ArgumentsNode* args) KJS_FAST_CALL
            : ExpressionNode(globalData)
            , m_expr(expr)
            , m_args(args)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecCall; }

    private:
        RefPtr<ExpressionNode> m_expr;
        RefPtr<ArgumentsNode> m_args;
    };

    class FunctionCallResolveNode : public ExpressionNode {
    public:
        FunctionCallResolveNode(JSGlobalData* globalData, const Identifier& ident, ArgumentsNode* args) KJS_FAST_CALL
            : ExpressionNode(globalData)
            , m_ident(ident)
            , m_args(args)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecCall; }

    protected:
        Identifier m_ident;
        RefPtr<ArgumentsNode> m_args;
        size_t m_index; // Used by LocalVarFunctionCallNode.
        size_t m_scopeDepth; // Used by ScopedVarFunctionCallNode and NonLocalVarFunctionCallNode
    };
    
    class FunctionCallBracketNode : public ExpressionNode {
    public:
        FunctionCallBracketNode(JSGlobalData* globalData, ExpressionNode* base, ExpressionNode* subscript, ArgumentsNode* args) KJS_FAST_CALL
            : ExpressionNode(globalData)
            , m_base(base)
            , m_subscript(subscript)
            , m_args(args)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecCall; }

    protected:
        RefPtr<ExpressionNode> m_base;
        RefPtr<ExpressionNode> m_subscript;
        RefPtr<ArgumentsNode> m_args;
    };

    class FunctionCallDotNode : public ExpressionNode {
    public:
        FunctionCallDotNode(JSGlobalData* globalData, ExpressionNode* base, const Identifier& ident, ArgumentsNode* args) KJS_FAST_CALL
            : ExpressionNode(globalData)
            , m_base(base)
            , m_ident(ident)
            , m_args(args)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecCall; }

    private:
        RefPtr<ExpressionNode> m_base;
        Identifier m_ident;
        RefPtr<ArgumentsNode> m_args;
    };

    class PrePostResolveNode : public ExpressionNode {
    public:
        PrePostResolveNode(JSGlobalData* globalData, const Identifier& ident) KJS_FAST_CALL
            : ExpressionNode(globalData, NumberType)
            , m_ident(ident)
        {
        }

    protected:
        Identifier m_ident;
        size_t m_index; // Used by LocalVarPostfixNode.
    };

    class PostIncResolveNode : public PrePostResolveNode {
    public:
        PostIncResolveNode(JSGlobalData* globalData, const Identifier& ident) KJS_FAST_CALL
            : PrePostResolveNode(globalData, ident)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecPostfix; }
    };

    class PostDecResolveNode : public PrePostResolveNode {
    public:
        PostDecResolveNode(JSGlobalData* globalData, const Identifier& ident) KJS_FAST_CALL
            : PrePostResolveNode(globalData, ident)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecPostfix; }
    };

    class PostfixBracketNode : public ExpressionNode {
    public:
        PostfixBracketNode(JSGlobalData* globalData, ExpressionNode* base, ExpressionNode* subscript) KJS_FAST_CALL
            : ExpressionNode(globalData)
            , m_base(base)
            , m_subscript(subscript)
        {
        }

        virtual Precedence precedence() const { return PrecPostfix; }

    protected:
        RefPtr<ExpressionNode> m_base;
        RefPtr<ExpressionNode> m_subscript;
    };

    class PostIncBracketNode : public PostfixBracketNode {
    public:
        PostIncBracketNode(JSGlobalData* globalData, ExpressionNode* base, ExpressionNode* subscript) KJS_FAST_CALL
            : PostfixBracketNode(globalData, base, subscript)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    };

    class PostDecBracketNode : public PostfixBracketNode {
    public:
        PostDecBracketNode(JSGlobalData* globalData, ExpressionNode* base, ExpressionNode* subscript) KJS_FAST_CALL
            : PostfixBracketNode(globalData, base, subscript)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    };

    class PostfixDotNode : public ExpressionNode {
    public:
        PostfixDotNode(JSGlobalData* globalData, ExpressionNode* base, const Identifier& ident) KJS_FAST_CALL
            : ExpressionNode(globalData)
            , m_base(base)
            , m_ident(ident)
        {
        }

        virtual Precedence precedence() const { return PrecPostfix; }

    protected:
        RefPtr<ExpressionNode> m_base;
        Identifier m_ident;
    };

    class PostIncDotNode : public PostfixDotNode {
    public:
        PostIncDotNode(JSGlobalData* globalData, ExpressionNode* base, const Identifier& ident) KJS_FAST_CALL
            : PostfixDotNode(globalData, base, ident)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    };

    class PostDecDotNode : public PostfixDotNode {
    public:
        PostDecDotNode(JSGlobalData* globalData, ExpressionNode*  base, const Identifier& ident) KJS_FAST_CALL
            : PostfixDotNode(globalData, base, ident)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    };

    class PostfixErrorNode : public ExpressionNode {
    public:
        PostfixErrorNode(JSGlobalData* globalData, ExpressionNode* expr, Operator oper) KJS_FAST_CALL
            : ExpressionNode(globalData)
            , m_expr(expr)
            , m_operator(oper)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecPostfix; }

    private:
        RefPtr<ExpressionNode> m_expr;
        Operator m_operator;
    };

    class DeleteResolveNode : public ExpressionNode {
    public:
        DeleteResolveNode(JSGlobalData* globalData, const Identifier& ident) KJS_FAST_CALL
            : ExpressionNode(globalData)
            , m_ident(ident)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecUnary; }

    private:
        Identifier m_ident;
    };

    class DeleteBracketNode : public ExpressionNode {
    public:
        DeleteBracketNode(JSGlobalData* globalData, ExpressionNode* base, ExpressionNode* subscript) KJS_FAST_CALL
            : ExpressionNode(globalData)
            , m_base(base)
            , m_subscript(subscript)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecUnary; }

    private:
        RefPtr<ExpressionNode> m_base;
        RefPtr<ExpressionNode> m_subscript;
    };

    class DeleteDotNode : public ExpressionNode {
    public:
        DeleteDotNode(JSGlobalData* globalData, ExpressionNode* base, const Identifier& ident) KJS_FAST_CALL
            : ExpressionNode(globalData)
            , m_base(base)
            , m_ident(ident)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecUnary; }

    private:
        RefPtr<ExpressionNode> m_base;
        Identifier m_ident;
    };

    class DeleteValueNode : public ExpressionNode {
    public:
        DeleteValueNode(JSGlobalData* globalData, ExpressionNode* expr) KJS_FAST_CALL
            : ExpressionNode(globalData)
            , m_expr(expr)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecUnary; }

    private:
        RefPtr<ExpressionNode> m_expr;
    };

    class VoidNode : public ExpressionNode {
    public:
        VoidNode(JSGlobalData* globalData, ExpressionNode* expr) KJS_FAST_CALL
            : ExpressionNode(globalData)
            , m_expr(expr)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecUnary; }

    private:
        RefPtr<ExpressionNode> m_expr;
    };

    class TypeOfResolveNode : public ExpressionNode {
    public:
        TypeOfResolveNode(JSGlobalData* globalData, const Identifier& ident) KJS_FAST_CALL
            : ExpressionNode(globalData, StringType)
            , m_ident(ident)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecUnary; }

        const Identifier& identifier() const KJS_FAST_CALL { return m_ident; }

    protected:
        Identifier m_ident;
        size_t m_index; // Used by LocalTypeOfNode.
    };

    class TypeOfValueNode : public ExpressionNode {
    public:
        TypeOfValueNode(JSGlobalData* globalData, ExpressionNode* expr) KJS_FAST_CALL
            : ExpressionNode(globalData, StringType)
            , m_expr(expr)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecUnary; }

    private:
        RefPtr<ExpressionNode> m_expr;
    };

    class PreIncResolveNode : public PrePostResolveNode {
    public:
        PreIncResolveNode(JSGlobalData* globalData, const Identifier& ident) KJS_FAST_CALL
            : PrePostResolveNode(globalData, ident)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecUnary; }
    };

    class PreDecResolveNode : public PrePostResolveNode {
    public:
        PreDecResolveNode(JSGlobalData* globalData, const Identifier& ident) KJS_FAST_CALL
            : PrePostResolveNode(globalData, ident)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecUnary; }
    };

    class PrefixBracketNode : public ExpressionNode {
    public:
        PrefixBracketNode(JSGlobalData* globalData, ExpressionNode* base, ExpressionNode* subscript) KJS_FAST_CALL
            : ExpressionNode(globalData)
            , m_base(base)
            , m_subscript(subscript)
        {
        }

        virtual Precedence precedence() const { return PrecUnary; }

    protected:
        RefPtr<ExpressionNode> m_base;
        RefPtr<ExpressionNode> m_subscript;
    };

    class PreIncBracketNode : public PrefixBracketNode {
    public:
        PreIncBracketNode(JSGlobalData* globalData, ExpressionNode* base, ExpressionNode* subscript) KJS_FAST_CALL
            : PrefixBracketNode(globalData, base, subscript)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    };

    class PreDecBracketNode : public PrefixBracketNode {
    public:
        PreDecBracketNode(JSGlobalData* globalData, ExpressionNode* base, ExpressionNode* subscript) KJS_FAST_CALL
            : PrefixBracketNode(globalData, base, subscript)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    };

    class PrefixDotNode : public ExpressionNode {
    public:
        PrefixDotNode(JSGlobalData* globalData, ExpressionNode* base, const Identifier& ident) KJS_FAST_CALL
            : ExpressionNode(globalData)
            , m_base(base)
            , m_ident(ident)
        {
        }

        virtual Precedence precedence() const { return PrecPostfix; }

    protected:
        RefPtr<ExpressionNode> m_base;
        Identifier m_ident;
    };

    class PreIncDotNode : public PrefixDotNode {
    public:
        PreIncDotNode(JSGlobalData* globalData, ExpressionNode* base, const Identifier& ident) KJS_FAST_CALL
            : PrefixDotNode(globalData, base, ident)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    };

    class PreDecDotNode : public PrefixDotNode {
    public:
        PreDecDotNode(JSGlobalData* globalData, ExpressionNode* base, const Identifier& ident) KJS_FAST_CALL
            : PrefixDotNode(globalData, base, ident)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    };

    class PrefixErrorNode : public ExpressionNode {
    public:
        PrefixErrorNode(JSGlobalData* globalData, ExpressionNode* expr, Operator oper) KJS_FAST_CALL
            : ExpressionNode(globalData)
            , m_expr(expr)
            , m_operator(oper)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecUnary; }

    private:
        RefPtr<ExpressionNode> m_expr;
        Operator m_operator;
    };

    class UnaryOpNode : public ExpressionNode {
    public:
        UnaryOpNode(JSGlobalData* globalData, ExpressionNode* expr)
            : ExpressionNode(globalData)
            , m_expr(expr)
        {
        }

        UnaryOpNode(JSGlobalData* globalData, JSType type, ExpressionNode* expr)
            : ExpressionNode(globalData, type)
            , m_expr(expr)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual OpcodeID opcode() const KJS_FAST_CALL = 0;

    protected:
        RefPtr<ExpressionNode> m_expr;
    };

    class UnaryPlusNode : public UnaryOpNode {
    public:
        UnaryPlusNode(JSGlobalData* globalData, ExpressionNode* expr) KJS_FAST_CALL
            : UnaryOpNode(globalData, NumberType, expr)
        {
        }

        virtual OpcodeID opcode() const KJS_FAST_CALL { return op_to_jsnumber; }
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecUnary; }
    };

    class NegateNode : public UnaryOpNode {
    public:
        NegateNode(JSGlobalData* globalData, ExpressionNode* expr) KJS_FAST_CALL
            : UnaryOpNode(globalData, NumberType, expr)
        {
        }

        virtual OpcodeID opcode() const KJS_FAST_CALL { return op_negate; }
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecUnary; }
    };

    class BitwiseNotNode : public UnaryOpNode {
    public:
        BitwiseNotNode(JSGlobalData* globalData, ExpressionNode* expr) KJS_FAST_CALL
            : UnaryOpNode(globalData, NumberType, expr)
        {
        }

        virtual OpcodeID opcode() const KJS_FAST_CALL { return op_bitnot; }
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecUnary; }
    };

    class LogicalNotNode : public UnaryOpNode {
    public:
        LogicalNotNode(JSGlobalData* globalData, ExpressionNode* expr) KJS_FAST_CALL
            : UnaryOpNode(globalData, BooleanType, expr)
        {
        }

        virtual OpcodeID opcode() const KJS_FAST_CALL { return op_not; }
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecUnary; }
    };

    class BinaryOpNode : public ExpressionNode {
    public:
        BinaryOpNode(JSGlobalData* globalData, ExpressionNode* term1, ExpressionNode* term2, bool rightHasAssignments)
            : ExpressionNode(globalData)
            , m_term1(term1)
            , m_term2(term2)
            , m_rightHasAssignments(rightHasAssignments)
        {
        }

        BinaryOpNode(JSGlobalData* globalData, JSType type, ExpressionNode* term1, ExpressionNode* term2, bool rightHasAssignments)
            : ExpressionNode(globalData, type)
            , m_term1(term1)
            , m_term2(term2)
            , m_rightHasAssignments(rightHasAssignments)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual OpcodeID opcode() const KJS_FAST_CALL = 0;

    protected:
        RefPtr<ExpressionNode> m_term1;
        RefPtr<ExpressionNode> m_term2;
        bool m_rightHasAssignments;
    };

    class ReverseBinaryOpNode : public ExpressionNode {
    public:
        ReverseBinaryOpNode(JSGlobalData* globalData, ExpressionNode* term1, ExpressionNode* term2, bool rightHasAssignments)
            : ExpressionNode(globalData)
            , m_term1(term1)
            , m_term2(term2)
            , m_rightHasAssignments(rightHasAssignments)
        {
        }

        ReverseBinaryOpNode(JSGlobalData* globalData, JSType type, ExpressionNode* term1, ExpressionNode* term2, bool rightHasAssignments)
            : ExpressionNode(globalData, type)
            , m_term1(term1)
            , m_term2(term2)
            , m_rightHasAssignments(rightHasAssignments)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual OpcodeID opcode() const KJS_FAST_CALL = 0;

    protected:
        RefPtr<ExpressionNode> m_term1;
        RefPtr<ExpressionNode> m_term2;
        bool m_rightHasAssignments;
    };

    class MultNode : public BinaryOpNode {
    public:
        MultNode(JSGlobalData* globalData, ExpressionNode* term1, ExpressionNode* term2, bool rightHasAssignments) KJS_FAST_CALL
            : BinaryOpNode(globalData, NumberType, term1, term2, rightHasAssignments)
        {
        }

        virtual OpcodeID opcode() const KJS_FAST_CALL { return op_mul; }
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecMultiplicative; }
    };

    class DivNode : public BinaryOpNode {
    public:
        DivNode(JSGlobalData* globalData, ExpressionNode* term1, ExpressionNode* term2, bool rightHasAssignments) KJS_FAST_CALL
            : BinaryOpNode(globalData, NumberType, term1, term2, rightHasAssignments)
        {
        }

        virtual OpcodeID opcode() const KJS_FAST_CALL { return op_div; }
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecMultiplicative; }
    };

    class ModNode : public BinaryOpNode {
    public:
        ModNode(JSGlobalData* globalData, ExpressionNode* term1, ExpressionNode* term2, bool rightHasAssignments) KJS_FAST_CALL
            : BinaryOpNode(globalData, NumberType, term1, term2, rightHasAssignments)
        {
        }

        virtual OpcodeID opcode() const KJS_FAST_CALL { return op_mod; }
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecMultiplicative; }
    };

    class AddNode : public BinaryOpNode {
    public:
        AddNode(JSGlobalData* globalData, ExpressionNode* term1, ExpressionNode* term2, bool rightHasAssignments) KJS_FAST_CALL
            : BinaryOpNode(globalData, term1, term2, rightHasAssignments)
        {
        }

        virtual OpcodeID opcode() const KJS_FAST_CALL { return op_add; }
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecAdditive; }
    };

    class SubNode : public BinaryOpNode {
    public:
        SubNode(JSGlobalData* globalData, ExpressionNode* term1, ExpressionNode* term2, bool rightHasAssignments) KJS_FAST_CALL
            : BinaryOpNode(globalData, term1, term2, rightHasAssignments)
        {
        }

        virtual OpcodeID opcode() const KJS_FAST_CALL { return op_sub; }
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecAdditive; }
    };

    class LeftShiftNode : public BinaryOpNode {
    public:
        LeftShiftNode(JSGlobalData* globalData, ExpressionNode* term1, ExpressionNode* term2, bool rightHasAssignments) KJS_FAST_CALL
            : BinaryOpNode(globalData, NumberType, term1, term2, rightHasAssignments)
        {
        }

        virtual OpcodeID opcode() const KJS_FAST_CALL { return op_lshift; }
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecShift; }
    };

    class RightShiftNode : public BinaryOpNode {
    public:
        RightShiftNode(JSGlobalData* globalData, ExpressionNode* term1, ExpressionNode* term2, bool rightHasAssignments) KJS_FAST_CALL
            : BinaryOpNode(globalData, NumberType, term1, term2, rightHasAssignments)
        {
        }

        virtual OpcodeID opcode() const KJS_FAST_CALL { return op_rshift; }
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecShift; }
    };

    class UnsignedRightShiftNode : public BinaryOpNode {
    public:
        UnsignedRightShiftNode(JSGlobalData* globalData, ExpressionNode* term1, ExpressionNode* term2, bool rightHasAssignments) KJS_FAST_CALL
            : BinaryOpNode(globalData, NumberType, term1, term2, rightHasAssignments)
        {
        }

        virtual OpcodeID opcode() const KJS_FAST_CALL { return op_urshift; }
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecShift; }
    };

    class LessNode : public BinaryOpNode {
    public:
        LessNode(JSGlobalData* globalData, ExpressionNode* term1, ExpressionNode* term2, bool rightHasAssignments) KJS_FAST_CALL
            : BinaryOpNode(globalData, BooleanType, term1, term2, rightHasAssignments)
        {
        }

        virtual OpcodeID opcode() const KJS_FAST_CALL { return op_less; }
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecRelational; }
    };

    class GreaterNode : public ReverseBinaryOpNode {
    public:
        GreaterNode(JSGlobalData* globalData, ExpressionNode* term1, ExpressionNode* term2, bool rightHasAssignments) KJS_FAST_CALL
            : ReverseBinaryOpNode(globalData, BooleanType, term1, term2, rightHasAssignments)
        {
        }

        virtual OpcodeID opcode() const KJS_FAST_CALL { return op_less; }
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecRelational; }
    };

    class LessEqNode : public BinaryOpNode {
    public:
        LessEqNode(JSGlobalData* globalData, ExpressionNode* term1, ExpressionNode* term2, bool rightHasAssignments) KJS_FAST_CALL
            : BinaryOpNode(globalData, BooleanType, term1, term2, rightHasAssignments)
        {
        }

        virtual OpcodeID opcode() const KJS_FAST_CALL { return op_lesseq; }
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecRelational; }
    };

    class GreaterEqNode : public ReverseBinaryOpNode {
    public:
        GreaterEqNode(JSGlobalData* globalData, ExpressionNode* term1, ExpressionNode* term2, bool rightHasAssignments) KJS_FAST_CALL
            : ReverseBinaryOpNode(globalData, BooleanType, term1, term2, rightHasAssignments)
        {
        }

        virtual OpcodeID opcode() const KJS_FAST_CALL { return op_lesseq; }
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecRelational; }
    };

    class InstanceOfNode : public BinaryOpNode {
    public:
        InstanceOfNode(JSGlobalData* globalData, ExpressionNode* term1, ExpressionNode* term2, bool rightHasAssignments) KJS_FAST_CALL
            : BinaryOpNode(globalData, BooleanType, term1, term2, rightHasAssignments)
        {
        }

        virtual OpcodeID opcode() const KJS_FAST_CALL { return op_instanceof; }
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecRelational; }
    };

    class InNode : public BinaryOpNode {
    public:
        InNode(JSGlobalData* globalData, ExpressionNode* term1, ExpressionNode* term2, bool rightHasAssignments) KJS_FAST_CALL
            : BinaryOpNode(globalData, term1, term2, rightHasAssignments)
        {
        }

        virtual OpcodeID opcode() const KJS_FAST_CALL { return op_in; }
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecRelational; }
    };

    class EqualNode : public BinaryOpNode {
    public:
        EqualNode(JSGlobalData* globalData, ExpressionNode* term1, ExpressionNode* term2, bool rightHasAssignments) KJS_FAST_CALL
            : BinaryOpNode(globalData, BooleanType, term1, term2, rightHasAssignments)
        {
        }

        virtual OpcodeID opcode() const KJS_FAST_CALL { return op_eq; }
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecEquality; }
    };

    class NotEqualNode : public BinaryOpNode {
    public:
        NotEqualNode(JSGlobalData* globalData, ExpressionNode* term1, ExpressionNode* term2, bool rightHasAssignments) KJS_FAST_CALL
            : BinaryOpNode(globalData, BooleanType, term1, term2, rightHasAssignments)
        {
        }

        virtual OpcodeID opcode() const KJS_FAST_CALL { return op_neq; }
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecEquality; }
    };

    class StrictEqualNode : public BinaryOpNode {
    public:
        StrictEqualNode(JSGlobalData* globalData, ExpressionNode* term1, ExpressionNode* term2, bool rightHasAssignments) KJS_FAST_CALL
            : BinaryOpNode(globalData, BooleanType, term1, term2, rightHasAssignments)
        {
        }

        virtual OpcodeID opcode() const KJS_FAST_CALL { return op_stricteq; }
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecEquality; }
    };

    class NotStrictEqualNode : public BinaryOpNode {
    public:
        NotStrictEqualNode(JSGlobalData* globalData, ExpressionNode* term1, ExpressionNode* term2, bool rightHasAssignments) KJS_FAST_CALL
            : BinaryOpNode(globalData, BooleanType, term1, term2, rightHasAssignments)
        {
        }

        virtual OpcodeID opcode() const KJS_FAST_CALL { return op_nstricteq; }
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecEquality; }
    };

    class BitAndNode : public BinaryOpNode {
    public:
        BitAndNode(JSGlobalData* globalData, ExpressionNode* term1, ExpressionNode* term2, bool rightHasAssignments) KJS_FAST_CALL
            : BinaryOpNode(globalData, NumberType, term1, term2, rightHasAssignments)
        {
        }

        virtual OpcodeID opcode() const KJS_FAST_CALL { return op_bitand; }
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecBitwiseAnd; }
    };

    class BitOrNode : public BinaryOpNode {
    public:
        BitOrNode(JSGlobalData* globalData, ExpressionNode* term1, ExpressionNode* term2, bool rightHasAssignments) KJS_FAST_CALL
            : BinaryOpNode(globalData, NumberType, term1, term2, rightHasAssignments)
        {
        }

        virtual OpcodeID opcode() const KJS_FAST_CALL { return op_bitor; }
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecBitwiseOr; }
    };

    class BitXOrNode : public BinaryOpNode {
    public:
        BitXOrNode(JSGlobalData* globalData, ExpressionNode* term1, ExpressionNode* term2, bool rightHasAssignments) KJS_FAST_CALL
            : BinaryOpNode(globalData, NumberType, term1, term2, rightHasAssignments)
        {
        }

        virtual OpcodeID opcode() const KJS_FAST_CALL { return op_bitxor; }
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecBitwiseXor; }
    };

    /**
     * m_expr1 && m_expr2, m_expr1 || m_expr2
     */
    class LogicalAndNode : public ExpressionNode {
    public:
        LogicalAndNode(JSGlobalData* globalData, ExpressionNode* expr1, ExpressionNode* expr2) KJS_FAST_CALL
            : ExpressionNode(globalData, BooleanType)
            , m_expr1(expr1)
            , m_expr2(expr2)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecLogicalAnd; }

    private:
        RefPtr<ExpressionNode> m_expr1;
        RefPtr<ExpressionNode> m_expr2;
    };

    class LogicalOrNode : public ExpressionNode {
    public:
        LogicalOrNode(JSGlobalData* globalData, ExpressionNode* expr1, ExpressionNode* expr2) KJS_FAST_CALL
            : ExpressionNode(globalData, BooleanType)
            , m_expr1(expr1)
            , m_expr2(expr2)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecLogicalOr; }

    private:
        RefPtr<ExpressionNode> m_expr1;
        RefPtr<ExpressionNode> m_expr2;
    };

    /**
     * The ternary operator, "m_logical ? m_expr1 : m_expr2"
     */
    class ConditionalNode : public ExpressionNode {
    public:
        ConditionalNode(JSGlobalData* globalData, ExpressionNode* logical, ExpressionNode* expr1, ExpressionNode* expr2) KJS_FAST_CALL
            : ExpressionNode(globalData)
            , m_logical(logical)
            , m_expr1(expr1)
            , m_expr2(expr2)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecConditional; }

    private:
        RefPtr<ExpressionNode> m_logical;
        RefPtr<ExpressionNode> m_expr1;
        RefPtr<ExpressionNode> m_expr2;
    };

    class ReadModifyResolveNode : public ExpressionNode {
    public:
        ReadModifyResolveNode(JSGlobalData* globalData, const Identifier& ident, Operator oper, ExpressionNode*  right, bool rightHasAssignments) KJS_FAST_CALL
            : ExpressionNode(globalData)
            , m_ident(ident)
            , m_right(right)
            , m_operator(oper)
            , m_rightHasAssignments(rightHasAssignments)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecAssignment; }

    protected:
        Identifier m_ident;
        RefPtr<ExpressionNode> m_right;
        size_t m_index; // Used by ReadModifyLocalVarNode.
        Operator m_operator : 31;
        bool m_rightHasAssignments : 1;
    };

    class AssignResolveNode : public ExpressionNode {
    public:
        AssignResolveNode(JSGlobalData* globalData, const Identifier& ident, ExpressionNode* right, bool rightHasAssignments) KJS_FAST_CALL
            : ExpressionNode(globalData)
            , m_ident(ident)
            , m_right(right)
            , m_rightHasAssignments(rightHasAssignments)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecAssignment; }

    protected:
        Identifier m_ident;
        RefPtr<ExpressionNode> m_right;
        size_t m_index; // Used by ReadModifyLocalVarNode.
        bool m_rightHasAssignments;
    };

    class ReadModifyBracketNode : public ExpressionNode {
    public:
        ReadModifyBracketNode(JSGlobalData* globalData, ExpressionNode* base, ExpressionNode* subscript, Operator oper, ExpressionNode* right, bool subscriptHasAssignments, bool rightHasAssignments) KJS_FAST_CALL
            : ExpressionNode(globalData)
            , m_base(base)
            , m_subscript(subscript)
            , m_right(right)
            , m_operator(oper)
            , m_subscriptHasAssignments(subscriptHasAssignments)
            , m_rightHasAssignments(rightHasAssignments)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecAssignment; }

    protected:
        RefPtr<ExpressionNode> m_base;
        RefPtr<ExpressionNode> m_subscript;
        RefPtr<ExpressionNode> m_right;
        Operator m_operator : 30;
        bool m_subscriptHasAssignments : 1;
        bool m_rightHasAssignments : 1;
    };

    class AssignBracketNode : public ExpressionNode {
    public:
        AssignBracketNode(JSGlobalData* globalData, ExpressionNode* base, ExpressionNode* subscript, ExpressionNode* right, bool subscriptHasAssignments, bool rightHasAssignments) KJS_FAST_CALL
            : ExpressionNode(globalData)
            , m_base(base)
            , m_subscript(subscript)
            , m_right(right)
            , m_subscriptHasAssignments(subscriptHasAssignments)
            , m_rightHasAssignments(rightHasAssignments)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecAssignment; }

    protected:
        RefPtr<ExpressionNode> m_base;
        RefPtr<ExpressionNode> m_subscript;
        RefPtr<ExpressionNode> m_right;
        bool m_subscriptHasAssignments : 1;
        bool m_rightHasAssignments : 1;
    };

    class AssignDotNode : public ExpressionNode {
    public:
        AssignDotNode(JSGlobalData* globalData, ExpressionNode* base, const Identifier& ident, ExpressionNode* right, bool rightHasAssignments) KJS_FAST_CALL
            : ExpressionNode(globalData)
            , m_base(base)
            , m_ident(ident)
            , m_right(right)
            , m_rightHasAssignments(rightHasAssignments)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecAssignment; }

    protected:
        RefPtr<ExpressionNode> m_base;
        Identifier m_ident;
        RefPtr<ExpressionNode> m_right;
        bool m_rightHasAssignments;
    };

    class ReadModifyDotNode : public ExpressionNode {
    public:
        ReadModifyDotNode(JSGlobalData* globalData, ExpressionNode* base, const Identifier& ident, Operator oper, ExpressionNode* right, bool rightHasAssignments) KJS_FAST_CALL
            : ExpressionNode(globalData)
            , m_base(base)
            , m_ident(ident)
            , m_right(right)
            , m_operator(oper)
            , m_rightHasAssignments(rightHasAssignments)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecAssignment; }

    protected:
        RefPtr<ExpressionNode> m_base;
        Identifier m_ident;
        RefPtr<ExpressionNode> m_right;
        Operator m_operator : 31;
        bool m_rightHasAssignments : 1;
    };

    class AssignErrorNode : public ExpressionNode {
    public:
        AssignErrorNode(JSGlobalData* globalData, ExpressionNode* left, Operator oper, ExpressionNode* right) KJS_FAST_CALL
            : ExpressionNode(globalData)
            , m_left(left)
            , m_operator(oper)
            , m_right(right)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecAssignment; }

    protected:
        RefPtr<ExpressionNode> m_left;
        Operator m_operator;
        RefPtr<ExpressionNode> m_right;
    };

    class CommaNode : public ExpressionNode {
    public:
        CommaNode(JSGlobalData* globalData, ExpressionNode* expr1, ExpressionNode* expr2) KJS_FAST_CALL
            : ExpressionNode(globalData)
            , m_expr1(expr1)
            , m_expr2(expr2)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecExpression; }

    private:
        RefPtr<ExpressionNode> m_expr1;
        RefPtr<ExpressionNode> m_expr2;
    };
    
    class VarDeclCommaNode : public CommaNode {
    public:
        VarDeclCommaNode(JSGlobalData* globalData, ExpressionNode* expr1, ExpressionNode* expr2) KJS_FAST_CALL
            : CommaNode(globalData, expr1, expr2)
        {
        }
        virtual Precedence precedence() const { return PrecAssignment; }
    };

    class ConstDeclNode : public ExpressionNode {
    public:
        ConstDeclNode(JSGlobalData* globalData, const Identifier& ident, ExpressionNode* in) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }
        PassRefPtr<ConstDeclNode> releaseNext() KJS_FAST_CALL { return m_next.release(); }

        Identifier m_ident;
        ListRefPtr<ConstDeclNode> m_next;
        RefPtr<ExpressionNode> m_init;
        
        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual RegisterID* emitCodeSingle(CodeGenerator&) KJS_FAST_CALL;
    };

    class ConstStatementNode : public StatementNode {
    public:
        ConstStatementNode(JSGlobalData* globalData, ConstDeclNode* next) KJS_FAST_CALL
            : StatementNode(globalData)
            , m_next(next)
        {
        }

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        
        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

    private:
        RefPtr<ConstDeclNode> m_next;
    };

    typedef Vector<RefPtr<StatementNode> > StatementVector;

    class SourceElements : public ParserRefCounted {
    public:
        SourceElements(JSGlobalData* globalData) : ParserRefCounted(globalData) {}

        void append(PassRefPtr<StatementNode>);
        void releaseContentsIntoVector(StatementVector& destination)
        {
            ASSERT(destination.isEmpty());
            m_statements.swap(destination);
        }

    private:
        StatementVector m_statements;
    };

    class BlockNode : public StatementNode {
    public:
        BlockNode(JSGlobalData*, SourceElements* children) KJS_FAST_CALL;

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;

        StatementVector& children() { return m_children; }

    protected:
        StatementVector m_children;
    };

    class EmptyStatementNode : public StatementNode {
    public:
        EmptyStatementNode(JSGlobalData* globalData) KJS_FAST_CALL // debug
            : StatementNode(globalData)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual bool isEmptyStatement() const KJS_FAST_CALL { return true; }
    };
    
    class DebuggerStatementNode : public StatementNode {
    public:
        DebuggerStatementNode(JSGlobalData* globalData) KJS_FAST_CALL
            : StatementNode(globalData)
        {
        }
        
        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
    };

    class ExprStatementNode : public StatementNode {
    public:
        ExprStatementNode(JSGlobalData* globalData, ExpressionNode* expr) KJS_FAST_CALL
            : StatementNode(globalData)
            , m_expr(expr)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;

    private:
        RefPtr<ExpressionNode> m_expr;
    };

    class VarStatementNode : public StatementNode {
    public:
        VarStatementNode(JSGlobalData* globalData, ExpressionNode* expr) KJS_FAST_CALL
            : StatementNode(globalData)
            , m_expr(expr)
        {
        }
        
        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;

    private:
        RefPtr<ExpressionNode> m_expr;
    };

    class IfNode : public StatementNode {
    public:
        IfNode(JSGlobalData* globalData, ExpressionNode* condition, StatementNode* ifBlock) KJS_FAST_CALL
            : StatementNode(globalData)
            , m_condition(condition)
            , m_ifBlock(ifBlock)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;

    protected:
        RefPtr<ExpressionNode> m_condition;
        RefPtr<StatementNode> m_ifBlock;
    };

    class IfElseNode : public IfNode {
    public:
        IfElseNode(JSGlobalData* globalData, ExpressionNode* condition, StatementNode* ifBlock, StatementNode* elseBlock) KJS_FAST_CALL
            : IfNode(globalData, condition, ifBlock)
            , m_elseBlock(elseBlock)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;

    private:
        RefPtr<StatementNode> m_elseBlock;
    };

    class DoWhileNode : public StatementNode {
    public:
        DoWhileNode(JSGlobalData* globalData, StatementNode* statement, ExpressionNode* expr) KJS_FAST_CALL
            : StatementNode(globalData)
            , m_statement(statement)
            , m_expr(expr)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;

    private:
        RefPtr<StatementNode> m_statement;
        RefPtr<ExpressionNode> m_expr;
    };

    class WhileNode : public StatementNode {
    public:
        WhileNode(JSGlobalData* globalData, ExpressionNode* expr, StatementNode* statement) KJS_FAST_CALL
            : StatementNode(globalData)
            , m_expr(expr)
            , m_statement(statement)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;

    private:
        RefPtr<ExpressionNode> m_expr;
        RefPtr<StatementNode> m_statement;
    };

    class ForNode : public StatementNode {
    public:
        ForNode(JSGlobalData* globalData, ExpressionNode* expr1, ExpressionNode* expr2, ExpressionNode* expr3, StatementNode* statement, bool expr1WasVarDecl) KJS_FAST_CALL
            : StatementNode(globalData)
            , m_expr1(expr1)
            , m_expr2(expr2)
            , m_expr3(expr3)
            , m_statement(statement)
            , m_expr1WasVarDecl(expr1 && expr1WasVarDecl)
        {
            ASSERT(statement);
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;

    private:
        RefPtr<ExpressionNode> m_expr1;
        RefPtr<ExpressionNode> m_expr2;
        RefPtr<ExpressionNode> m_expr3;
        RefPtr<StatementNode> m_statement;
        bool m_expr1WasVarDecl;
    };

    class ForInNode : public StatementNode {
    public:
        ForInNode(JSGlobalData*, ExpressionNode*, ExpressionNode*, StatementNode*) KJS_FAST_CALL;
        ForInNode(JSGlobalData*, const Identifier&, ExpressionNode*, ExpressionNode*, StatementNode*) KJS_FAST_CALL;
        
        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;

    private:
        Identifier m_ident;
        RefPtr<ExpressionNode> m_init;
        RefPtr<ExpressionNode> m_lexpr;
        RefPtr<ExpressionNode> m_expr;
        RefPtr<StatementNode> m_statement;
        bool m_identIsVarDecl;
    };

    class ContinueNode : public StatementNode {
    public:
        ContinueNode(JSGlobalData* globalData) KJS_FAST_CALL
            : StatementNode(globalData)
        {
        }

        ContinueNode(JSGlobalData* globalData, const Identifier& ident) KJS_FAST_CALL
            : StatementNode(globalData)
            , m_ident(ident)
        {
        }
        
        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;

    private:
        Identifier m_ident;
    };

    class BreakNode : public StatementNode {
    public:
        BreakNode(JSGlobalData* globalData) KJS_FAST_CALL
            : StatementNode(globalData)
        {
        }

        BreakNode(JSGlobalData* globalData, const Identifier& ident) KJS_FAST_CALL
            : StatementNode(globalData)
            , m_ident(ident)
        {
        }
        
        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;

    private:
        Identifier m_ident;
    };

    class ReturnNode : public StatementNode {
    public:
        ReturnNode(JSGlobalData* globalData, ExpressionNode* value) KJS_FAST_CALL
            : StatementNode(globalData)
            , m_value(value)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual bool isReturnNode() const KJS_FAST_CALL { return true; }

    private:
        RefPtr<ExpressionNode> m_value;
    };

    class WithNode : public StatementNode {
    public:
        WithNode(JSGlobalData* globalData, ExpressionNode* expr, StatementNode* statement) KJS_FAST_CALL
            : StatementNode(globalData)
            , m_expr(expr)
            , m_statement(statement)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;

    private:
        RefPtr<ExpressionNode> m_expr;
        RefPtr<StatementNode> m_statement;
    };

    class LabelNode : public StatementNode {
    public:
        LabelNode(JSGlobalData* globalData, const Identifier& label, StatementNode* statement) KJS_FAST_CALL
            : StatementNode(globalData)
            , m_label(label)
            , m_statement(statement)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual void pushLabel(const Identifier& ident) KJS_FAST_CALL { m_statement->pushLabel(ident); }

    private:
        Identifier m_label;
        RefPtr<StatementNode> m_statement;
    };

    class ThrowNode : public StatementNode {
    public:
        ThrowNode(JSGlobalData* globalData, ExpressionNode* expr) KJS_FAST_CALL
            : StatementNode(globalData)
            , m_expr(expr)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;

    private:
        RefPtr<ExpressionNode> m_expr;
    };

    class TryNode : public StatementNode {
    public:
        TryNode(JSGlobalData* globalData, StatementNode* tryBlock, const Identifier& exceptionIdent, StatementNode* catchBlock, StatementNode* finallyBlock) KJS_FAST_CALL
            : StatementNode(globalData)
            , m_tryBlock(tryBlock)
            , m_exceptionIdent(exceptionIdent)
            , m_catchBlock(catchBlock)
            , m_finallyBlock(finallyBlock)
        {
        }

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* dst = 0) KJS_FAST_CALL;

    private:
        RefPtr<StatementNode> m_tryBlock;
        Identifier m_exceptionIdent;
        RefPtr<StatementNode> m_catchBlock;
        RefPtr<StatementNode> m_finallyBlock;
    };

    class ParameterNode : public Node {
    public:
        ParameterNode(JSGlobalData* globalData, const Identifier& ident) KJS_FAST_CALL
            : Node(globalData)
            , m_ident(ident)
        {
        }

        ParameterNode(JSGlobalData* globalData, ParameterNode* l, const Identifier& ident) KJS_FAST_CALL
            : Node(globalData)
            , m_ident(ident)
        {
            l->m_next = this;
        }

        Identifier ident() KJS_FAST_CALL { return m_ident; }
        ParameterNode *nextParam() KJS_FAST_CALL { return m_next.get(); }
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        PassRefPtr<ParameterNode> releaseNext() KJS_FAST_CALL { return m_next.release(); }
        virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }

    private:
        friend class FuncDeclNode;
        friend class FuncExprNode;
        Identifier m_ident;
        ListRefPtr<ParameterNode> m_next;
    };

    class ScopeNode : public BlockNode {
    public:
        ScopeNode(JSGlobalData*, SourceElements*, VarStack*, FunctionStack*, bool usesEval, bool needsClosure) KJS_FAST_CALL;

        int sourceId() const KJS_FAST_CALL { return m_sourceId; }
        const UString& sourceURL() const KJS_FAST_CALL { return m_sourceURL; }
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        
        bool usesEval() const { return m_usesEval; }
        bool needsClosure() const { return m_needsClosure; }
        
        VarStack& varStack() { return m_varStack; }
        FunctionStack& functionStack() { return m_functionStack; }
        
    protected:
        VarStack m_varStack;
        FunctionStack m_functionStack;

    private:
        UString m_sourceURL;
        int m_sourceId;
        bool m_usesEval;
        bool m_needsClosure;
    };

    class ProgramNode : public ScopeNode {
    public:
        static ProgramNode* create(JSGlobalData*, SourceElements*, VarStack*, FunctionStack*, bool usesEval, bool needsClosure) KJS_FAST_CALL;

        ProgramCodeBlock& code(ScopeChainNode* scopeChain) KJS_FAST_CALL
        {
            if (!m_code)
                generateCode(scopeChain);
            return *m_code;
        }

    private:
        ProgramNode(JSGlobalData*, SourceElements*, VarStack*, FunctionStack*, bool usesEval, bool needsClosure) KJS_FAST_CALL;

        void generateCode(ScopeChainNode*) KJS_FAST_CALL;
        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        Vector<size_t> m_varIndexes; // Storage indexes belonging to the nodes in m_varStack. (Recorded to avoid double lookup.)
        Vector<size_t> m_functionIndexes; // Storage indexes belonging to the nodes in m_functionStack. (Recorded to avoid double lookup.)

        OwnPtr<ProgramCodeBlock> m_code;
    };

    class EvalNode : public ScopeNode {
    public:
        static EvalNode* create(JSGlobalData*, SourceElements*, VarStack*, FunctionStack*, bool usesEval, bool needsClosure) KJS_FAST_CALL;

        EvalCodeBlock& code(ScopeChainNode* scopeChain) KJS_FAST_CALL
        {
            if (!m_code)
                generateCode(scopeChain);
            return *m_code;
        }

    private:
        EvalNode(JSGlobalData*, SourceElements*, VarStack*, FunctionStack*, bool usesEval, bool needsClosure) KJS_FAST_CALL;

        void generateCode(ScopeChainNode*) KJS_FAST_CALL;
        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        OwnPtr<EvalCodeBlock> m_code;
    };

    class FunctionBodyNode : public ScopeNode {
    public:
        static FunctionBodyNode* create(JSGlobalData*, SourceElements*, VarStack*, FunctionStack*, bool usesEval, bool needsClosure) KJS_FAST_CALL;

        Vector<Identifier>& parameters() KJS_FAST_CALL { return m_parameters; }
        UString paramString() const KJS_FAST_CALL;

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        
        SymbolTable& symbolTable() { return m_symbolTable; } // FIXME: Remove this
        
        CodeBlock& code(ScopeChainNode* scopeChain) KJS_FAST_CALL
        {
            ASSERT(scopeChain);
            if (!m_code)
                generateCode(scopeChain);
            return *m_code;
        }

        CodeBlock& generatedCode() KJS_FAST_CALL
        {
            ASSERT(m_code);
            return *m_code;
        }

        void mark();

        void setSource(const SourceRange& source) { m_source = source; } 
        UString toSourceString() const KJS_FAST_CALL { return UString("{") + m_source.toString() + UString("}"); }

    protected:
        FunctionBodyNode(JSGlobalData*, SourceElements*, VarStack*, FunctionStack*, bool usesEval, bool needsClosure) KJS_FAST_CALL;

    private:
        void generateCode(ScopeChainNode*) KJS_FAST_CALL;
        
        Vector<Identifier> m_parameters;
        SymbolTable m_symbolTable;
        OwnPtr<CodeBlock> m_code;
        SourceRange m_source;
    };

    class FuncExprNode : public ExpressionNode {
    public:
        FuncExprNode(JSGlobalData* globalData, const Identifier& ident, FunctionBodyNode* body, const SourceRange& source, ParameterNode* parameter = 0) KJS_FAST_CALL
            : ExpressionNode(globalData)
            , m_ident(ident)
            , m_parameter(parameter)
            , m_body(body)
        {
            addParams();
            m_body->setSource(source);
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;
        JSFunction* makeFunction(ExecState*, ScopeChainNode*) KJS_FAST_CALL;
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { return PrecMember; }
        virtual bool needsParensIfLeftmost() const { return true; }

        FunctionBodyNode* body() { return m_body.get(); }

    private:
        void addParams() KJS_FAST_CALL;

        // Used for streamTo
        friend class PropertyNode;
        Identifier m_ident;
        RefPtr<ParameterNode> m_parameter;
        RefPtr<FunctionBodyNode> m_body;
    };

    class FuncDeclNode : public StatementNode {
    public:
        FuncDeclNode(JSGlobalData* globalData, const Identifier& ident, FunctionBodyNode* body, const SourceRange& source, ParameterNode* parameter = 0) KJS_FAST_CALL
            : StatementNode(globalData)
            , m_ident(ident)
            , m_parameter(parameter)
            , m_body(body)
        {
            addParams();
            m_body->setSource(source);
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        JSFunction* makeFunction(ExecState*, ScopeChainNode*) KJS_FAST_CALL;

        Identifier m_ident;

        FunctionBodyNode* body() { return m_body.get(); }

    private:
        void addParams() KJS_FAST_CALL;

        RefPtr<ParameterNode> m_parameter;
        RefPtr<FunctionBodyNode> m_body;
    };

    class CaseClauseNode : public Node {
    public:
        CaseClauseNode(JSGlobalData* globalData, ExpressionNode* expr) KJS_FAST_CALL
            : Node(globalData)
            , m_expr(expr)
        {
        }

        CaseClauseNode(JSGlobalData* globalData, ExpressionNode* expr, SourceElements* children) KJS_FAST_CALL
            : Node(globalData)
            , m_expr(expr)
        {
            if (children)
                children->releaseContentsIntoVector(m_children);
        }

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }

        ExpressionNode* expr() const { return m_expr.get(); }
        StatementVector& children() { return m_children; }

    private:
        RefPtr<ExpressionNode> m_expr;
        StatementVector m_children;
    };

    class ClauseListNode : public Node {
    public:
        ClauseListNode(JSGlobalData* globalData, CaseClauseNode* clause) KJS_FAST_CALL
            : Node(globalData)
            , m_clause(clause)
        {
        }

        ClauseListNode(JSGlobalData* globalData, ClauseListNode* clauseList, CaseClauseNode* clause) KJS_FAST_CALL
            : Node(globalData)
            , m_clause(clause)
        {
            clauseList->m_next = this;
        }

        CaseClauseNode* getClause() const KJS_FAST_CALL { return m_clause.get(); }
        ClauseListNode* getNext() const KJS_FAST_CALL { return m_next.get(); }
        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        PassRefPtr<ClauseListNode> releaseNext() KJS_FAST_CALL { return m_next.release(); }
        virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }

    private:
        friend class CaseBlockNode;
        RefPtr<CaseClauseNode> m_clause;
        ListRefPtr<ClauseListNode> m_next;
    };

    class CaseBlockNode : public Node {
    public:
        CaseBlockNode(JSGlobalData* globalData, ClauseListNode* list1, CaseClauseNode* defaultClause, ClauseListNode* list2) KJS_FAST_CALL
            : Node(globalData)
            , m_list1(list1)
            , m_defaultClause(defaultClause)
            , m_list2(list2)
        {
        }

        RegisterID* emitCodeForBlock(CodeGenerator&, RegisterID* input, RegisterID* dst = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;
        virtual Precedence precedence() const { ASSERT_NOT_REACHED(); return PrecExpression; }

    private:
        RefPtr<ClauseListNode> m_list1;
        RefPtr<CaseClauseNode> m_defaultClause;
        RefPtr<ClauseListNode> m_list2;
    };

    class SwitchNode : public StatementNode {
    public:
        SwitchNode(JSGlobalData* globalData, ExpressionNode* expr, CaseBlockNode* block) KJS_FAST_CALL
            : StatementNode(globalData)
            , m_expr(expr)
            , m_block(block)
        {
        }

        virtual RegisterID* emitCode(CodeGenerator&, RegisterID* = 0) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;

    private:
        RefPtr<ExpressionNode> m_expr;
        RefPtr<CaseBlockNode> m_block;
    };

    class BreakpointCheckStatement : public StatementNode {
    public:
        BreakpointCheckStatement(JSGlobalData*, PassRefPtr<StatementNode>) KJS_FAST_CALL;

        virtual void streamTo(SourceStream&) const KJS_FAST_CALL;

    private:
        RefPtr<StatementNode> m_statement;
    };

    struct ElementList {
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
        ElementNode* head;
        ElementNode* tail;
    };

    struct PropertyList {
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
        PropertyListNode* head;
        PropertyListNode* tail;
    };

    struct ArgumentList {
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
        ArgumentListNode* head;
        ArgumentListNode* tail;
    };

    struct ConstDeclList {
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
        ConstDeclNode* head;
        ConstDeclNode* tail;
    };

    struct ParameterList {
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
        ParameterNode* head;
        ParameterNode* tail;
    };

    struct ClauseList {
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
        ClauseListNode* head;
        ClauseListNode* tail;
    };

} // namespace KJS

#endif // NODES_H_
