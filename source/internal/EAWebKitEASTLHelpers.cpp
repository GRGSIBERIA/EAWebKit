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
// EAWebKitEASTLStringHelpers.cpp
// By Arpit Baldeva (Isolated Paul Pedriana's code in EAWebkit.h/cpp)
///////////////////////////////////////////////////////////////////////////////

#include <EAWebKit/internal/EAWebKitEASTLHelpers.h>
#include <EAIO/FnEncode.h> 

namespace EA
{
    namespace WebKit
	{
		void ConvertToString8(const FixedString16& s16, FixedString8& s8)
		{
			// A 16 bit string of strlen n16 will convert to an 8 bit UTF8 string with a strlen >= n16.
			eastl_size_t size16 = s16.size();
			eastl_size_t size8  = size16 + 8;   // Give it some extra space to detect that Strlcpy needed more space..

			// Most of the time we are copying ascii text and the dest length == source length.
			s8.resize(size8);
			size_t destLen = EA::IO::StrlcpyUTF16ToUTF8(&s8[0], size8 + 1, s16.c_str(), size16); // +1 because there is a 0 char at the end that is included in the string capacity.

			if(destLen > size16) // If there were multibyte expansions, and thus that we need to use a dest size > source size...
			{
				destLen = EA::IO::StrlcpyUTF16ToUTF8(NULL, 0, s16.c_str(), size16); // Call with NULL in order to get the required strlen.
				s8.resize((eastl_size_t)destLen);
				EA::IO::StrlcpyUTF16ToUTF8(&s8[0], destLen + 1, s16.c_str(), size16);
			}
			else
				s8.resize((eastl_size_t)destLen);
		}


		void ConvertToString16(const FixedString8& s8,  FixedString16& s16)
		{
			// An 8 bit UTF8 string with a strlen of n8 will convert to a 16 bit string with a strlen <= n8.
			eastl_size_t size8 = s8.size();

			// Most of the time we are copying ascii text and the dest length == source length.
			s16.resize(size8);
			const size_t destLen = EA::IO::StrlcpyUTF8ToUTF16(&s16[0], size8 + 1, s8.c_str(), size8); // +1 because there is a 0 char at the end that is included in the string capacity.
			s16.resize((eastl_size_t)destLen);
		}




	}
} // namespace EA







