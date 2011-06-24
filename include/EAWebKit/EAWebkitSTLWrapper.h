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
// EAWebkitSTLWrapper.h
// By Arpit Baldeva
///////////////////////////////////////////////////////////////////////////////


#ifndef EAWEBKIT_EAWEBKITSTLWRAPPER_H
#define EAWEBKIT_EAWEBKITSTLWRAPPER_H


///////////////////////////////////////////////////////////////////////
// The purpose of this file is to stop the template expansion inside the main app exe(and thus exe contain code due to the template instantiation) for the users of the EAWebkit.
// This is required for LGPL compliance.
// We keep all the stl related classes in this single file to start with. It can be broken down if necessary in future.
///////////////////////////////////////////////////////////////////////

#include <EABase/eabase.h>
namespace EA
{
    namespace WebKit
    {
		//This wraps eastl::fixed_string<char8_t,  256, true, EASTLAllocator>.
		class EASTLFixedString8Wrapper
		{
		public:
			EASTLFixedString8Wrapper();
			EASTLFixedString8Wrapper(const char8_t* str);
			EASTLFixedString8Wrapper(const EASTLFixedString8Wrapper& rhs);
			EASTLFixedString8Wrapper& operator = (const EASTLFixedString8Wrapper& rhs);
			virtual ~EASTLFixedString8Wrapper();
			void* GetImpl() const
			{
				return mString8;
			}

			const char8_t* c_str() const;
		private:
			void* mString8;
		};

		//This wraps eastl::fixed_string<char16_t,  256, true, EASTLAllocator>.
		class EASTLFixedString16Wrapper
		{
		public:
			EASTLFixedString16Wrapper();
			EASTLFixedString16Wrapper(const char16_t* str);
			EASTLFixedString16Wrapper(const EASTLFixedString16Wrapper& rhs);
			EASTLFixedString16Wrapper& operator = (const EASTLFixedString16Wrapper& rhs);
			virtual ~EASTLFixedString16Wrapper();
			void* GetImpl() const
			{
				return mString16;
			}

			const char16_t* c_str() const;
		private:
			void* mString16;
		};
		
		//This wraps typedef eastl::fixed_multimap<FixedString16, FixedString16, 8, true, fstr_iless, EASTLAllocator> HeaderMap;
		class EASTLHeaderMapWrapper
		{
		public:
			EASTLHeaderMapWrapper();
			EASTLHeaderMapWrapper(const EASTLHeaderMapWrapper& rhs);
			EASTLHeaderMapWrapper& operator = (const EASTLHeaderMapWrapper& rhs);
			virtual ~EASTLHeaderMapWrapper();
			void* GetImpl() const
			{
				return mHeaderMap;
			}
		private:
			void* mHeaderMap;
		};

    }
}


#endif // Header include guard
