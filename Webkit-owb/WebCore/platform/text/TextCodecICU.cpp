/*
 * Copyright (C) 2004, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov <ap@nypop.com>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

#include "config.h"
#include "TextCodecICU.h"

#include "CharacterNames.h"
#include "CString.h"
#include "PlatformString.h"
#include <unicode/ucnv.h>
#include <unicode/ucnv_cb.h>
#include <wtf/Assertions.h>

using std::auto_ptr;
using std::min;

namespace WebCore {

const size_t ConversionBufferSize = 16384;

static UConverter* cachedConverterICU;

static auto_ptr<TextCodec> newTextCodecICU(const TextEncoding& encoding, const void*)
{
    return auto_ptr<TextCodec>(new TextCodecICU(encoding));
}

void TextCodecICU::registerBaseEncodingNames(EncodingNameRegistrar registrar)
{
    registrar("UTF-8", "UTF-8");
}

void TextCodecICU::registerBaseCodecs(TextCodecRegistrar registrar)
{
    registrar("UTF-8", newTextCodecICU, 0);
}

// FIXME: Registering all the encodings we get from ucnv_getAvailableName
// includes encodings we don't want or need. For example: UTF16_PlatformEndian,
// UTF16_OppositeEndian, UTF32_PlatformEndian, UTF32_OppositeEndian, and all
// the encodings with commas and version numbers.

void TextCodecICU::registerExtendedEncodingNames(EncodingNameRegistrar registrar)
{
    // We register Hebrew with logical ordering using a separate name.
    // Otherwise, this would share the same canonical name as the
    // visual ordering case, and then TextEncoding could not tell them
    // apart; ICU works with either name.
    registrar("ISO-8859-8-I", "ISO-8859-8-I");

    int32_t numEncodings = ucnv_countAvailable();
    for (int32_t i = 0; i < numEncodings; ++i) {
        const char* name = ucnv_getAvailableName(i);
        UErrorCode error = U_ZERO_ERROR;
        // FIXME: Should we use the "MIME" standard instead of "IANA"?
        const char* standardName = ucnv_getStandardName(name, "IANA", &error);
        if (!U_SUCCESS(error) || !standardName)
            continue;

        // 1. Treat GB2312 encoding as GBK (its more modern superset), to match other browsers.
        // 2. On the Web, GB2312 is encoded as EUC-CN or HZ, while ICU provides a native encoding
        //    for encoding GB_2312-80 and several others. So, we need to override this behavior, too.
        if (strcmp(standardName, "GB2312") == 0 || strcmp(standardName, "GB_2312-80") == 0)
            standardName = "GBK";
        // Similarly, EUC-KR encodings all map to an extended version.
        else if (strcmp(standardName, "KS_C_5601-1987") == 0 || strcmp(standardName, "EUC-KR") == 0)
            standardName = "windows-949-2000";
        // And so on.
        else if (strcmp(standardName, "ISO_8859-9:1989") == 0)
            standardName = "windows-1254";
        else if (strcmp(standardName, "TIS-620") == 0)
            standardName = "windows-874-2000";

        registrar(standardName, standardName);

        uint16_t numAliases = ucnv_countAliases(name, &error);
        ASSERT(U_SUCCESS(error));
        if (U_SUCCESS(error))
            for (uint16_t j = 0; j < numAliases; ++j) {
                error = U_ZERO_ERROR;
                const char* alias = ucnv_getAlias(name, j, &error);
                ASSERT(U_SUCCESS(error));
                if (U_SUCCESS(error) && alias != standardName)
                    registrar(alias, standardName);
            }
    }

    // Additional aliases.
    // Perhaps we can get these added to ICU.
    registrar("macroman", "macintosh");
    registrar("xmacroman", "macintosh");

    // Additional aliases that historically were present in the encoding
    // table in WebKit on Macintosh that don't seem to be present in ICU.
    // Perhaps we can prove these are not used on the web and remove them.
    // Or perhaps we can get them added to ICU.
    registrar("cnbig5", "Big5");
    registrar("cngb", "EUC-CN");
    registrar("csISO88598I", "ISO_8859-8-I");
    registrar("csgb231280", "EUC-CN");
    registrar("dos720", "cp864");
    registrar("dos874", "cp874");
    registrar("jis7", "ISO-2022-JP");
    registrar("koi", "KOI8-R");
    registrar("logical", "ISO-8859-8-I");
    registrar("unicode11utf8", "UTF-8");
    registrar("unicode20utf8", "UTF-8");
    registrar("visual", "ISO-8859-8");
    registrar("winarabic", "windows-1256");
    registrar("winbaltic", "windows-1257");
    registrar("wincyrillic", "windows-1251");
    registrar("windows874", "windows874-2000");
    registrar("iso885911", "windows874-2000");
    registrar("wingreek", "windows-1253");
    registrar("winhebrew", "windows-1255");
    registrar("winlatin2", "windows-1250");
    registrar("winturkish", "windows-1254");
    registrar("winvietnamese", "windows-1258");
    registrar("xcp1250", "windows-1250");
    registrar("xcp1251", "windows-1251");
    registrar("xeuc", "EUC-JP");
    registrar("xeuccn", "EUC-CN");
    registrar("xgbk", "EUC-CN");
    registrar("xunicode20utf8", "UTF-8");
    registrar("xxbig5", "Big5");
}

void TextCodecICU::registerExtendedCodecs(TextCodecRegistrar registrar)
{
    // See comment above in registerEncodingNames.
    registrar("ISO-8859-8-I", newTextCodecICU, 0);

    int32_t numEncodings = ucnv_countAvailable();
    for (int32_t i = 0; i < numEncodings; ++i) {
        const char* name = ucnv_getAvailableName(i);
        UErrorCode error = U_ZERO_ERROR;
        // FIXME: Should we use the "MIME" standard instead of "IANA"?
        const char* standardName = ucnv_getStandardName(name, "IANA", &error);
        if (!U_SUCCESS(error) || !standardName)
            continue;
        registrar(standardName, newTextCodecICU, 0);
    }
}

TextCodecICU::TextCodecICU(const TextEncoding& encoding)
    : m_encoding(encoding)
    , m_numBufferedBytes(0)
    , m_converterICU(0)
    , m_needsGBKFallbacks(false)
{
}

TextCodecICU::~TextCodecICU()
{
    releaseICUConverter();
}

void TextCodecICU::releaseICUConverter() const
{
    if (m_converterICU) {
        if (cachedConverterICU)
            ucnv_close(cachedConverterICU);
        cachedConverterICU = m_converterICU;
        m_converterICU = 0;
    }
}

void TextCodecICU::createICUConverter() const
{
    ASSERT(!m_converterICU);

    const char* name = m_encoding.name();
    m_needsGBKFallbacks = name[0] == 'G' && name[1] == 'B' && name[2] == 'K' && !name[3];

    UErrorCode err;

    if (cachedConverterICU) {
        err = U_ZERO_ERROR;
        const char* cachedName = ucnv_getName(cachedConverterICU, &err);
        if (U_SUCCESS(err) && m_encoding == cachedName) {
            m_converterICU = cachedConverterICU;
            cachedConverterICU = 0;
            return;
        }
    }

    err = U_ZERO_ERROR;
    m_converterICU = ucnv_open(m_encoding.name(), &err);
#if !LOG_DISABLED
    if (err == U_AMBIGUOUS_ALIAS_WARNING)
        LOG_ERROR("ICU ambiguous alias warning for encoding: %s", m_encoding.name());
#endif
    if (m_converterICU)
        ucnv_setFallback(m_converterICU, TRUE);
}

int TextCodecICU::decodeToBuffer(UChar* target, UChar* targetLimit, const char*& source, const char* sourceLimit, int32_t* offsets, bool flush, UErrorCode& err)
{
    UChar* targetStart = target;
    err = U_ZERO_ERROR;
    ucnv_toUnicode(m_converterICU, &target, targetLimit, &source, sourceLimit, offsets, flush, &err);
    return target - targetStart;
}
#include <wtf/FastAllocBase.h>

class ErrorCallbackSetter: public WTF::FastAllocBase {
public:
    ErrorCallbackSetter(UConverter* converter, bool stopOnError)
        : m_converter(converter)
        , m_shouldStopOnEncodingErrors(stopOnError)
    {
        if (m_shouldStopOnEncodingErrors) {
            UErrorCode err = U_ZERO_ERROR;
            ucnv_setToUCallBack(m_converter, UCNV_TO_U_CALLBACK_SUBSTITUTE,
                           UCNV_SUB_STOP_ON_ILLEGAL, &m_savedAction,
                           &m_savedContext, &err);
            ASSERT(err == U_ZERO_ERROR);
        }
    }
    ~ErrorCallbackSetter()
    {
        if (m_shouldStopOnEncodingErrors) {
            UErrorCode err = U_ZERO_ERROR;
            const void* oldContext;
            UConverterToUCallback oldAction;
            ucnv_setToUCallBack(m_converter, m_savedAction,
                   m_savedContext, &oldAction,
                   &oldContext, &err);
            ASSERT(oldAction == UCNV_TO_U_CALLBACK_SUBSTITUTE);
            ASSERT(!strcmp(static_cast<const char*>(oldContext), UCNV_SUB_STOP_ON_ILLEGAL));
            ASSERT(err == U_ZERO_ERROR);
        }
    }
private:
    UConverter* m_converter;
    bool m_shouldStopOnEncodingErrors;
    const void* m_savedContext;
    UConverterToUCallback m_savedAction;
};

String TextCodecICU::decode(const char* bytes, size_t length, bool flush, bool stopOnError, bool& sawError)
{
    // Get a converter for the passed-in encoding.
    if (!m_converterICU) {
        createICUConverter();
        ASSERT(m_converterICU);
        if (!m_converterICU) {
            LOG_ERROR("error creating ICU encoder even though encoding was in table");
            return String();
        }
    }
    
    ErrorCallbackSetter callbackSetter(m_converterICU, stopOnError);

    Vector<UChar> result;

    UChar buffer[ConversionBufferSize];
    UChar* bufferLimit = buffer + ConversionBufferSize;
    const char* source = reinterpret_cast<const char*>(bytes);
    const char* sourceLimit = source + length;
    int32_t* offsets = NULL;
    UErrorCode err = U_ZERO_ERROR;

    do {
        int ucharsDecoded = decodeToBuffer(buffer, bufferLimit, source, sourceLimit, offsets, flush, err);
        result.append(buffer, ucharsDecoded);
    } while (err == U_BUFFER_OVERFLOW_ERROR);

    if (U_FAILURE(err)) {
        // flush the converter so it can be reused, and not be bothered by this error.
        do {
            decodeToBuffer(buffer, bufferLimit, source, sourceLimit, offsets, true, err);
        } while (source < sourceLimit);
        sawError = true;
    }

    String resultString = String::adopt(result);

    // <http://bugs.webkit.org/show_bug.cgi?id=17014>
    // Simplified Chinese pages use the code A3A0 to mean "full-width space", but ICU decodes it as U+E5E5.
    if (m_encoding == "GBK" || m_encoding == "gb18030")
        resultString.replace(0xE5E5, ideographicSpace);

    return resultString;
}

// We need to apply these fallbacks ourselves as they are not currently supported by ICU and
// they were provided by the old TEC encoding path
// Needed to fix <rdar://problem/4708689>
static UChar getGbkEscape(UChar32 codePoint)
{
    switch (codePoint) {
        case 0x01F9:
            return 0xE7C8;
        case 0x1E3F:
            return 0xE7C7;
        case 0x22EF:
            return 0x2026;
        case 0x301C:
            return 0xFF5E;
        default:
            return 0;
    }
}

// Invalid character handler when writing escaped entities for unrepresentable
// characters. See the declaration of TextCodec::encode for more.
static void urlEscapedEntityCallback(const void* context, UConverterFromUnicodeArgs* fromUArgs, const UChar* codeUnits, int32_t length,
                                     UChar32 codePoint, UConverterCallbackReason reason, UErrorCode* err)
{
    if (reason == UCNV_UNASSIGNED) {
        *err = U_ZERO_ERROR;

        UnencodableReplacementArray entity;
        int entityLen = TextCodec::getUnencodableReplacement(codePoint, URLEncodedEntitiesForUnencodables, entity);
        ucnv_cbFromUWriteBytes(fromUArgs, entity, entityLen, 0, err);
    } else
        UCNV_FROM_U_CALLBACK_ESCAPE(context, fromUArgs, codeUnits, length, codePoint, reason, err);
}

// Substitutes special GBK characters, escaping all other unassigned entities.
static void gbkCallbackEscape(const void* context, UConverterFromUnicodeArgs* fromUArgs, const UChar* codeUnits, int32_t length,
                              UChar32 codePoint, UConverterCallbackReason reason, UErrorCode* err) 
{
    UChar outChar;
    if (reason == UCNV_UNASSIGNED && (outChar = getGbkEscape(codePoint))) {
        const UChar* source = &outChar;
        *err = U_ZERO_ERROR;
        ucnv_cbFromUWriteUChars(fromUArgs, &source, source + 1, 0, err);
        return;
    }
    UCNV_FROM_U_CALLBACK_ESCAPE(context, fromUArgs, codeUnits, length, codePoint, reason, err);
}

// Combines both gbkUrlEscapedEntityCallback and GBK character substitution.
static void gbkUrlEscapedEntityCallack(const void* context, UConverterFromUnicodeArgs* fromUArgs, const UChar* codeUnits, int32_t length,
                                       UChar32 codePoint, UConverterCallbackReason reason, UErrorCode* err) 
{
    if (reason == UCNV_UNASSIGNED) {
        if (UChar outChar = getGbkEscape(codePoint)) {
            const UChar* source = &outChar;
            *err = U_ZERO_ERROR;
            ucnv_cbFromUWriteUChars(fromUArgs, &source, source + 1, 0, err);
            return;
        }
        urlEscapedEntityCallback(context, fromUArgs, codeUnits, length, codePoint, reason, err);
        return;
    }
    UCNV_FROM_U_CALLBACK_ESCAPE(context, fromUArgs, codeUnits, length, codePoint, reason, err);
}

static void gbkCallbackSubstitute(const void* context, UConverterFromUnicodeArgs* fromUArgs, const UChar* codeUnits, int32_t length,
                                  UChar32 codePoint, UConverterCallbackReason reason, UErrorCode* err) 
{
    UChar outChar;
    if (reason == UCNV_UNASSIGNED && (outChar = getGbkEscape(codePoint))) {
        const UChar* source = &outChar;
        *err = U_ZERO_ERROR;
        ucnv_cbFromUWriteUChars(fromUArgs, &source, source + 1, 0, err);
        return;
    }
    UCNV_FROM_U_CALLBACK_SUBSTITUTE(context, fromUArgs, codeUnits, length, codePoint, reason, err);
}

CString TextCodecICU::encode(const UChar* characters, size_t length, UnencodableHandling handling)
{
    if (!length)
        return "";

    if (!m_converterICU)
        createICUConverter();
    if (!m_converterICU)
        return CString();

    // FIXME: We should see if there is "force ASCII range" mode in ICU;
    // until then, we change the backslash into a yen sign.
    // Encoding will change the yen sign back into a backslash.
    String copy(characters, length);
    copy.replace('\\', m_encoding.backslashAsCurrencySymbol());

    const UChar* source = copy.characters();
    const UChar* sourceLimit = source + copy.length();

    UErrorCode err = U_ZERO_ERROR;

    switch (handling) {
        case QuestionMarksForUnencodables:
            ucnv_setSubstChars(m_converterICU, "?", 1, &err);
            ucnv_setFromUCallBack(m_converterICU, m_needsGBKFallbacks ? gbkCallbackSubstitute : UCNV_FROM_U_CALLBACK_SUBSTITUTE, 0, 0, 0, &err);
            break;
        case EntitiesForUnencodables:
            ucnv_setFromUCallBack(m_converterICU, m_needsGBKFallbacks ? gbkCallbackEscape : UCNV_FROM_U_CALLBACK_ESCAPE, UCNV_ESCAPE_XML_DEC, 0, 0, &err);
            break;
        case URLEncodedEntitiesForUnencodables:
            ucnv_setFromUCallBack(m_converterICU, m_needsGBKFallbacks ? gbkUrlEscapedEntityCallack : urlEscapedEntityCallback, 0, 0, 0, &err);
            break;
    }

    ASSERT(U_SUCCESS(err));
    if (U_FAILURE(err))
        return CString();

    Vector<char> result;
    size_t size = 0;
    do {
        char buffer[ConversionBufferSize];
        char* target = buffer;
        char* targetLimit = target + ConversionBufferSize;
        err = U_ZERO_ERROR;
        ucnv_fromUnicode(m_converterICU, &target, targetLimit, &source, sourceLimit, 0, true, &err);
        size_t count = target - buffer;
        result.grow(size + count);
        memcpy(result.data() + size, buffer, count);
        size += count;
    } while (err == U_BUFFER_OVERFLOW_ERROR);

    return CString(result.data(), size);
}


} // namespace WebCore
