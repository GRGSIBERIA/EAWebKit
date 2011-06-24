/*
Copyright (C) 2009 Electronic Arts, Inc.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1.  Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
2.  Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
3.  Neither the name of Electronic Arts, Inc. ("EA") nor the names of
    its contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY ELECTRONIC ARTS AND ITS CONTRIBUTORS "AS IS" AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL ELECTRONIC ARTS OR ITS CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

///////////////////////////////////////////////////////////////////////////////
// EAWebKitEASTLHelpers.h
// By Arpit Baldeva (Isolated Paul Pedriana's code in EAWebkit.h)
///////////////////////////////////////////////////////////////////////////////


#ifndef EAWEBKIT_EAWEBKITEASTLHELPERS_H
#define EAWEBKIT_EAWEBKITEASTLHELPERS_H


///////////////////////////////////////////////////////////////////////
// The purpose of this file is to isolate the EASTL related code from EAWebkit.h.
// This is required for LGPL compliance.
///////////////////////////////////////////////////////////////////////

#include <EABase/eabase.h>
#include <EASTL/list.h>
#include <EASTL/fixed_string.h>
#include <EASTL/fixed_map.h>
#include <EAWebKit/EAWebkitAllocator.h> //For EASTLAllocator

#if EAWEBKIT_THROW_BUILD_ERROR
#error This file should be included only in a dll build
#endif

namespace EA
{
    namespace WebKit
    {
		///////////////////////////////////////////////////////////////////////
		// EAWebKit strings
		///////////////////////////////////////////////////////////////////////

		typedef eastl::fixed_string<char16_t, 256, true, EASTLAllocator> FixedString16;
		typedef eastl::fixed_string<char8_t,  256, true, EASTLAllocator> FixedString8;

		typedef eastl::fixed_string<char16_t, 32, true, EASTLAllocator> FixedString32_16;
		typedef eastl::fixed_string<char8_t,  32, true, EASTLAllocator> FixedString32_8;

		void ConvertToString8 (const FixedString16& s16, FixedString8&  s8);
		void ConvertToString16(const FixedString8&  s8,  FixedString16& s16);

		//Does not belong here. Copy here only to help in port.

		///////////////////////////////////////////////////////////////////////
		// HeaderMap 
		///////////////////////////////////////////////////////////////////////

		// Used for HTTP header entries.
		struct fstr_iless : public eastl::binary_function<FixedString16, FixedString16, bool>
		{
			bool operator()(const FixedString16& a, const FixedString16& b) const { return (a.comparei(b) < 0); }
		};

		// Used for HeaderMap::find_as() calls.
		struct str_iless : public eastl::binary_function<FixedString16, const char16_t*, bool>
		{
			bool operator()(const FixedString16& a, const char16_t*      b) const { return (a.comparei(b) < 0); }
			bool operator()(const char16_t*      b, const FixedString16& a) const { return (a.comparei(b) > 0); }
		};

		typedef eastl::fixed_multimap<FixedString16, FixedString16, 8, true, fstr_iless, EASTLAllocator> HeaderMap;
		
		#define GET_FIXEDSTRING8(wrapper) (reinterpret_cast<EA::WebKit::FixedString8*>((wrapper).GetImpl()))
		#define GET_FIXEDSTRING16(wrapper) (reinterpret_cast<EA::WebKit::FixedString16*>((wrapper).GetImpl()))
		#define GET_HEADERMAP(wrapper) (reinterpret_cast<EA::WebKit::HeaderMap*>((wrapper).GetImpl()))

    }
}

#endif // Header include guard
