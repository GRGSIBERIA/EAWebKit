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

// Copyright (c) 2006 Electronic Arts Inc. All rights reserved.

#include "EAAssert/eaassert.h"

#ifndef EA_ASSERT_HAVE_OWN_HEADER

    #include <stdio.h>

    #ifdef EA_PLATFORM_WINDOWS
        #if !defined(_WIN32_WINNT) || (_WIN32_WINNT < 0x0400)
            #undef  _WIN32_WINNT
            #define _WIN32_WINNT 0x0400
        #endif
        #ifndef WIN32_LEAN_AND_MEAN
            #define WIN32_LEAN_AND_MEAN
        #endif
        #pragma warning(push,0)
        #include <windows.h>        // ::IsDebuggerPresent
        #pragma warning(pop)
    #endif

#if !defined(EA_ASSERT_VSNPRINTF)
#if defined(_MSC_VER)
#define EA_ASSERT_VSNPRINTF _vsnprintf
#define EA_ASSERT_SNPRINTF _snprintf
#else
#define EA_ASSERT_VSNPRINTF vsnprintf
#define EA_ASSERT_SNPRINTF snprintf
#endif
#endif

namespace EA { 
    namespace Assert { 
    namespace Detail {
    namespace {

        bool DefaultFailureCallback(const char* expr, const char* filename, int line, const char* function, const char* msg, va_list args)
        {
            const int largeEnough = 2048;
            char output[largeEnough + 1] = {};
            char fmtMsg[largeEnough + 1] = {};

            int len = EA_ASSERT_VSNPRINTF(fmtMsg, largeEnough, msg, args);

            // different platforms return different values for the error, but in both
            // cases it'll be out of bounds, so clamp the return value to largeEnough.
            if (len < 0 || len > largeEnough)
                len = largeEnough;

            fmtMsg[len] = '\0';

            len = EA_ASSERT_SNPRINTF(output, largeEnough,
                "%s(%d) : EA_ASSERT failed: '%s' in function: %s\n, message: %s",
                filename, line, expr, function, fmtMsg);
            if (len < 0 || len > largeEnough)
                len = largeEnough;

            output[len] = '\0';


#if defined(EA_DEBUG) || defined(_DEBUG) 

    #if defined(EA_PLATFORM_WINDOWS)
            if (::IsDebuggerPresent())
            {

                OutputDebugString(output);
            }
            else
      #endif
                puts(output);
#endif
            return true;
        }


        FailureCallback gFailureCallback = &DefaultFailureCallback; 
    }}

    void SetFailureCallback(FailureCallback failureCallback)
    {
        Detail::gFailureCallback = failureCallback;
    }

    FailureCallback GetFailureCallback()
    {
        return Detail::gFailureCallback;
    }

    }}

#endif
