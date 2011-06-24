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

#include "config.h"
#include "JSGlobalData.h"

#include "collector.h"
#include "CommonIdentifiers.h"
#include "lexer.h"
#include "list.h"
#include "lookup.h"
#include "Machine.h"
#include "nodes.h"
#include "Parser.h"

#if USE(MULTIPLE_THREADS)
#include <wtf/Threading.h>
#include <wtf/ThreadSpecific.h>
#endif

using namespace WTF;

namespace KJS {

extern const HashTable arrayTable;
extern const HashTable dateTable;
extern const HashTable mathTable;
extern const HashTable numberTable;
extern const HashTable regExpTable;
extern const HashTable regExpConstructorTable;
extern const HashTable stringTable;


JSGlobalData::JSGlobalData()
    : machine(new Machine)
    , heap(new Heap(machine))
#if USE(MULTIPLE_THREADS)
    , arrayTable(new HashTable(KJS::arrayTable))
    , dateTable(new HashTable(KJS::dateTable))
    , mathTable(new HashTable(KJS::mathTable))
    , numberTable(new HashTable(KJS::numberTable))
    , regExpTable(new HashTable(KJS::regExpTable))
    , regExpConstructorTable(new HashTable(KJS::regExpConstructorTable))
    , stringTable(new HashTable(KJS::stringTable))
#else
    , arrayTable(&KJS::arrayTable)
    , dateTable(&KJS::dateTable)
    , mathTable(&KJS::mathTable)
    , numberTable(&KJS::numberTable)
    , regExpTable(&KJS::regExpTable)
    , regExpConstructorTable(&KJS::regExpConstructorTable)
    , stringTable(&KJS::stringTable)
#endif
    , identifierTable(createIdentifierTable())
    , propertyNames(new CommonIdentifiers(this))
    , newParserObjects(0)
    , parserObjectExtraRefCounts(0)
    , lexer(new Lexer(this))
    , parser(new Parser)
    , head(0)
{
}

JSGlobalData::~JSGlobalData()
{
    // Modified by Paul Pedriana 1/17/2009 in order to fix memory leaks.
    delete heap;
    delete machine;

#if USE(MULTIPLE_THREADS)
    delete[] arrayTable->table;
    delete[] dateTable->table;
    delete[] mathTable->table;
    delete[] numberTable->table;
    delete[] regExpTable->table;
    delete[] regExpConstructorTable->table;
    delete[] stringTable->table;

    delete arrayTable;
    delete dateTable;
    delete mathTable;
    delete numberTable;
    delete regExpTable;
    delete regExpConstructorTable;
    delete stringTable;
#endif

    delete parser;
    delete lexer;

    delete propertyNames;
    deleteIdentifierTable(identifierTable);

    delete newParserObjects;
    delete parserObjectExtraRefCounts;
}


// Modified by Paul Pedriana 1/17/2009 in order to fix memory leaks.
#if USE(MULTIPLE_THREADS)
    // We don't use MULTIPLE_THREADS at EA and so we don't try to figure out how to do this.
#else
    static JSGlobalData* gThreadInstance = NULL;
    static JSGlobalData* gSharedInstance = NULL;
#endif


JSGlobalData& JSGlobalData::threadInstance()
{
    #if USE(MULTIPLE_THREADS)
        static ThreadSpecific<JSGlobalData> sharedInstance;
        return *sharedInstance;
    #else
        if(!gThreadInstance)
            gThreadInstance = new JSGlobalData;
        return *gThreadInstance;
    #endif
}

JSGlobalData& JSGlobalData::sharedInstance()
{
    #if USE(MULTIPLE_THREADS)
        AtomicallyInitializedStatic(JSGlobalData, sharedInstance);
        return sharedInstance;
    #else
        if(!gSharedInstance)
            gSharedInstance = new JSGlobalData;
        return *gSharedInstance;
    #endif
}

void JSGlobalData::staticFinalize()
{
    #if USE(MULTIPLE_THREADS)
        // We don't use MULTIPLE_THREADS at EA and so we don't try to figure out how to do this.
    #else
        delete gThreadInstance;
        gThreadInstance = NULL;

        delete gSharedInstance;
        gSharedInstance = NULL;
    #endif
}

}

