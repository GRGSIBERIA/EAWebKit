/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
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

#include "config.h"
#include "JSTreeWalker.h"

#include "JSNode.h"
#include "Node.h"
#include "NodeFilter.h"
#include "TreeWalker.h"

using namespace KJS;

namespace WebCore {
    
void JSTreeWalker::mark()
{
    if (NodeFilter* filter = m_impl->filter())
        filter->mark();
    
    DOMObject::mark();
}
    
JSValue* JSTreeWalker::parentNode(ExecState* exec, const ArgList& args)
{
    JSValue* exception = 0;
    Node* node = impl()->parentNode(exception);
    if (exception) {
        exec->setException(exception);
        return jsUndefined();
    }
    return toJS(exec, node);
}
    
JSValue* JSTreeWalker::firstChild(ExecState* exec, const ArgList& args)
{
    JSValue* exception = 0;
    Node* node = impl()->firstChild(exception);
    if (exception) {
        exec->setException(exception);
        return jsUndefined();
    }
    return toJS(exec, node);
}
    
JSValue* JSTreeWalker::lastChild(ExecState* exec, const ArgList& args)
{
    JSValue* exception = 0;
    Node* node = impl()->lastChild(exception);
    if (exception) {
        exec->setException(exception);
        return jsUndefined();
    }
    return toJS(exec, node);
}
    
JSValue* JSTreeWalker::nextSibling(ExecState* exec, const ArgList& args)
{
    JSValue* exception = 0;
    Node* node = impl()->nextSibling(exception);
    if (exception) {
        exec->setException(exception);
        return jsUndefined();
    }
    return toJS(exec, node);
}
    
JSValue* JSTreeWalker::previousSibling(ExecState* exec, const ArgList& args)
{
    JSValue* exception = 0;
    Node* node = impl()->previousSibling(exception);
    if (exception) {
        exec->setException(exception);
        return jsUndefined();
    }
    return toJS(exec, node);
}
    
JSValue* JSTreeWalker::previousNode(ExecState* exec, const ArgList& args)
{
    JSValue* exception = 0;
    Node* node = impl()->previousNode(exception);
    if (exception) {
        exec->setException(exception);
        return jsUndefined();
    }
    return toJS(exec, node);
}
    
JSValue* JSTreeWalker::nextNode(ExecState* exec, const ArgList& args)
{
    JSValue* exception = 0;
    Node* node = impl()->nextNode(exception);
    if (exception) {
        exec->setException(exception);
        return jsUndefined();
    }
    return toJS(exec, node);
}

}
