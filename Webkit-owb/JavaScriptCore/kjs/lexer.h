// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2007 Apple Inc.
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

#ifndef Lexer_h
#define Lexer_h

#include <wtf/FastAllocBase.h>
#include "lookup.h"
#include "ustring.h"
#include <wtf/Vector.h>
#include "SourceRange.h"

namespace KJS {

  class Identifier;
  class RegExp;

  class Lexer : Noncopyable {
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
    void setCode(int startingLineNumber, PassRefPtr<SourceProvider> source);
    int lex(void* lvalp, void* llocp);

    int lineNo() const { return yylineno; }

    bool prevTerminator() const { return terminator; }

    enum State { Start,
                 IdentifierOrKeyword,
                 Identifier,
                 InIdentifierOrKeyword,
                 InIdentifier,
                 InIdentifierStartUnicodeEscapeStart,
                 InIdentifierStartUnicodeEscape,
                 InIdentifierPartUnicodeEscapeStart,
                 InIdentifierPartUnicodeEscape,
                 InSingleLineComment,
                 InMultiLineComment,
                 InNum,
                 InNum0,
                 InHex,
                 InOctal,
                 InDecimal,
                 InExponentIndicator,
                 InExponent,
                 Hex,
                 Octal,
                 Number,
                 String,
                 Eof,
                 InString,
                 InEscapeSequence,
                 InHexEscape,
                 InUnicodeEscape,
                 Other,
                 Bad };

    bool scanRegExp();
    const UString& pattern() const { return m_pattern; }
    const UString& flags() const { return m_flags; }

    static unsigned char convertHex(int);
    static unsigned char convertHex(int c1, int c2);
    static UChar convertUnicode(int c1, int c2, int c3, int c4);
    static bool isIdentStart(int);
    static bool isIdentPart(int);
    static bool isHexDigit(int);

    bool sawError() const { return error; }

    void clear();
    SourceRange sourceRange(int openBrace, int closeBrace) { return SourceRange(m_source, openBrace + 1, closeBrace); }

  private:
    friend struct JSGlobalData;
    Lexer(JSGlobalData*);
    ~Lexer();

    int yylineno;
    bool done;
    Vector<char> m_buffer8;
    Vector<UChar> m_buffer16;
    bool terminator;
    bool restrKeyword;
    // encountered delimiter like "'" and "}" on last run
    bool delimited;
    bool skipLF;
    bool skipCR;
    bool eatNextIdentifier;
    int stackToken;
    int lastToken;

    State state;
    void setDone(State);
    unsigned int pos;
    void shift(unsigned int p);
    void nextLine();
    int lookupKeyword(const char *);

    bool isWhiteSpace() const;
    bool isLineTerminator();
    static bool isOctalDigit(int);

    int matchPunctuator(int& charPos, int c1, int c2, int c3, int c4);
    static unsigned short singleEscape(unsigned short);
    static unsigned short convertOctal(int c1, int c2, int c3);

    void record8(int);
    void record16(int);
    void record16(UChar);

    KJS::Identifier* makeIdentifier(const Vector<UChar>& buffer);
    UString* makeUString(const Vector<UChar>& buffer);

    RefPtr<SourceProvider> m_source;
    const UChar* code;
    unsigned int length;
    int yycolumn;
    int atLineStart;
    bool error;

    // current and following unicode characters (int to allow for -1 for end-of-file marker)
    int current, next1, next2, next3;

    Vector<UString*> m_strings;
    Vector<KJS::Identifier*> m_identifiers;

    JSGlobalData* m_globalData;

    UString m_pattern;
    UString m_flags;

    const HashTable& mainTable;
  };

} // namespace KJS

#endif // Lexer_h
