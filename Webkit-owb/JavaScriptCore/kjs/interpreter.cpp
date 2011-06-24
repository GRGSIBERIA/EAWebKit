/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2007 Apple Inc.
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

#include "config.h"
#include "interpreter.h"

#include "ExecState.h"
#include "JSGlobalObject.h"
#include "Machine.h"
#include "Parser.h"
#include "completion.h"
#include "debugger.h"
#include <profiler/Profiler.h>
#include <stdio.h>
#include <EAWebKit/EAWebKitView.h>

#if !PLATFORM(WIN_OS) && !PLATFORM(XBOX) && !PLATFORM(PS3) 
#include <unistd.h>
#endif

namespace KJS {

Completion Interpreter::checkSyntax(ExecState* exec, const UString& sourceURL, int startingLineNumber, const UString& code)
{
    return checkSyntax(exec, sourceURL, startingLineNumber, UStringSourceProvider::create(code));
}

Completion Interpreter::checkSyntax(ExecState* exec, const UString& sourceURL, int startingLineNumber, PassRefPtr<SourceProvider> source)
{
    JSLock lock;

    int errLine;
    UString errMsg;

    RefPtr<ProgramNode> progNode = exec->parser()->parse<ProgramNode>(exec, sourceURL, startingLineNumber, source, 0, &errLine, &errMsg);
    if (!progNode)
        return Completion(Throw, Error::create(exec, SyntaxError, errMsg, errLine, 0, sourceURL));
    return Completion(Normal);
}

Completion Interpreter::evaluate(ExecState* exec, ScopeChain& scopeChain, const UString& sourceURL, int startingLineNumber, const UString& code, JSValue* thisV)
{
    return evaluate(exec, scopeChain, sourceURL, startingLineNumber, UStringSourceProvider::create(code), thisV);
}

Completion Interpreter::evaluate(ExecState* exec, ScopeChain& scopeChain, const UString& sourceURL, int startingLineNumber, PassRefPtr<SourceProvider> source, JSValue* thisValue)
{
    JSLock lock;
    
    // parse the source code
    int sourceId;
    int errLine;
    UString errMsg;


    // 11/10/09 CSidhall - Added notify process start to user 
	NOTIFY_PROCESS_STATUS(EA::WebKit::kVProcessTypeJavaScriptParser, EA::WebKit::kVProcessStatusStarted);
	
    RefPtr<ProgramNode> programNode = exec->parser()->parse<ProgramNode>(exec, sourceURL, startingLineNumber, source, &sourceId, &errLine, &errMsg);

	NOTIFY_PROCESS_STATUS(EA::WebKit::kVProcessTypeJavaScriptParser, EA::WebKit::kVProcessStatusEnded);


    // no program node means a syntax error occurred
    if (!programNode)
        return Completion(Throw, Error::create(exec, SyntaxError, errMsg, errLine, sourceId, sourceURL));


	NOTIFY_PROCESS_STATUS(EA::WebKit::kVProcessTypeJavaScriptExecute, EA::WebKit::kVProcessStatusStarted);
	
    JSObject* thisObj = (!thisValue || thisValue->isUndefinedOrNull()) ? exec->dynamicGlobalObject() : thisValue->toObject(exec);

    JSValue* exception = 0;
    JSValue* result = exec->machine()->execute(programNode.get(), exec, scopeChain.node(), thisObj, &exception);


	NOTIFY_PROCESS_STATUS(EA::WebKit::kVProcessTypeJavaScriptExecute, EA::WebKit::kVProcessStatusEnded);


    if (exception) {
        if (exception->isObject() && static_cast<JSObject*>(exception)->isWatchdogException())
            return Completion(Interrupted, result);
        return Completion(Throw, exception);
    }
    return Completion(Normal, result);
}

static bool printExceptions = false;

bool Interpreter::shouldPrintExceptions()
{
    return printExceptions;
}

void Interpreter::setShouldPrintExceptions(bool print)
{
    printExceptions = print;
}

} // namespace KJS
