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

#ifndef PPMALLOC_DLLINFO_H
#define PPMALLOC_DLLINFO_H


#ifdef _MSC_VER
    #pragma once
#endif


///////////////////////////////////////////////////////////////////////////////
// Version
//
// The version is a 5 digit number: Major:MinorMinor:PatchPatch.
// A value of 11005 refers to version 1.10.05.
//
#define PPM_GENERAL_ALLOCATOR_VERSION       11100
#define PPM_GENERAL_ALLOCATOR_DEBUG_VERSION 11100



#if (defined(EA_DLL) || defined(PPMALLOC_DLL)) && defined(_MSC_VER)
    // If PPM_API is not defined, assume this package is being
    // included by something else. When PPMalloc is built as a DLL, 
    // PPM_API is defined to __declspec(dllexport) by the
    // package build script.
    #if !defined(PPM_API)
        #define PPM_API          __declspec(dllimport)
        #define PPM_TEMPLATE_API
    #endif
#else
    // PPM_API has no meaning except for DLL builds.
    #define PPM_API
    #define PPM_TEMPLATE_API
#endif


#endif  // include guard

