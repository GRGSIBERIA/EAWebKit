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

/////////////////////////////////////////////////////////////////////////////
// BCCookieManagerEA.h
// Written/Modified by Arpit Baldeva
//
// Implements cookies for EAWebkit as per RFC 2901, 2965 and original
// Netscape cookie specifications.
/////////////////////////////////////////////////////////////////////////////

#ifndef BCCookieManagerEA_h
#define BCCookieManagerEA_h

#include "config.h"

#include <wtf/FastAllocBase.h>


#include <EASTL/string.h>
#include <EASTL/list.h>
#ifdef USE_EATHREAD_LIBRARY
    #include <eathread/eathread_futex.h>
#endif

#include <EAWebKit/EAWebKit.h>
#include <EAWebKit/internal/EAWebKitEASTLHelpers.h>
#include <time.h>

namespace WKAL{
        class ResourceHandleManager;
        class KURL;
}

namespace EA
{

    namespace WebKit
    {
        class TransportInfo;
        

        struct CookieManagerParameters: public WTF::FastAllocBase
        {
            static const uint32_t kMaxIndividualCookieSizeDefault =  4096;  // RFC 2109 specifies that it be at least this.
            static const uint32_t kMaxCookieFileSizeDefault       = 32768;  // This is arbitrary.
            static const uint32_t kMaxCookieCountDefault          =    256;  // This is arbitrary.

            CookieManagerParameters()
                : mCookieFilePath()
                , mMaxIndividualCookieSize(kMaxIndividualCookieSizeDefault)
                , mMaxCookieFileSize(kMaxCookieFileSizeDefault)
                , mMaxCookieCount(kMaxCookieCountDefault)
            {
            }
            
            explicit CookieManagerParameters(const char* cookieFilePath,
                                             uint32_t maxIndividualCookieSize = kMaxIndividualCookieSizeDefault,
                                             uint32_t maxCookieFileSize = kMaxCookieFileSizeDefault,
                                             uint32_t maxCookieCount = kMaxCookieCountDefault)
                : mCookieFilePath(cookieFilePath ? cookieFilePath : "")
                , mMaxIndividualCookieSize(maxIndividualCookieSize)
                , mMaxCookieFileSize(maxCookieFileSize)
                , mMaxCookieCount((uint16_t)maxCookieCount)
            {
            }
            
            FixedString8 mCookieFilePath;           // File path for persistent cookies. The meaning of the path is user-defined. It's not limited to a system disk file path.
            uint32_t     mMaxIndividualCookieSize;  // The maximum size of an individual cookie.
            uint32_t     mMaxCookieFileSize;        // Max size of persistent cookie file.
            uint16_t     mMaxCookieCount;           // Max number of concurrent cookies.
        };

        typedef FixedString8 CookieFullTextFixedString8;


        /// Cookie
        ///
        /// class for holding cookie data stored by CookieManager
        ///
        class Cookie : public WTF::FastAllocBase
        {
        public:
            static const uint16_t MAX_NUM_PORTS = 16;
            
            // To consider: These FixedString8 objects use a lot of memory, and most of the time it isn't needed. Consider making a leaner version.
            FixedString8  mName;
            FixedString8  mValue;
            FixedString8  mDomain;
            FixedString8  mPath;
            FixedString8  mComment;
            FixedString8  mCommentURL;

            uint32_t      mPorts[MAX_NUM_PORTS];
            time_t	      mExpirationTime;
            uint16_t      mVersion;
            bool          mDomainDefaulted;
            bool          mPathDefaulted;
            bool          mExpiresAtEndOfSession;    
            bool          mSecure;

        public:
            explicit Cookie(char8_t* name)
                : mName(name)
                , mValue()
                , mDomain()
                , mPath()
                , mComment()
                , mCommentURL()
                , mExpirationTime(0x7fffffff)//Note by Arpit Baldeva- 32 bit max int would be sufficient for our purpose until I can figure out a easy way to figure out max time held by time_t. //64-bit - 0x7fffffffffffffff)
                , mVersion(0)//By default, it is Netscape original behavior. 1 is for rfc2109/rfc2965 update.  It seems to be required attribute now but...seems like nobody cares.
                , mDomainDefaulted(true)
                , mPathDefaulted(true)
                , mExpiresAtEndOfSession(true)
                , mSecure(false)
            {
                mPorts[0] = 0;
            }

            bool IsValidPort(uint32_t portNum) const
            {
                if(mPorts[0] == 0) //All ports are allowed.
                    return true;
                for(unsigned i=0; i<MAX_NUM_PORTS && mPorts[i]; i++){
                    if(mPorts[i] == portNum)
                        return true;
                }
                return false;
            }

            // Cookie buffer is by default 512 bytes. FixedString8 is 256. We can tackle this later if it becomes a problem.
            // It would just involve changing CookieFulltextFixedString8 typedef.
            // As such, I don't see teams using even 256 bytes for a cookie.
            // Following may be different from what you received from server (in case path/domain were defaulted)
            CookieFullTextFixedString8 CreateCookieString() const;

            bool IsCookieExpired(time_t timeNow) const;
        };


        class CookiePathSorter
        {
        public:
            bool operator() (const Cookie* cookie1, const Cookie* cookie2) const
            {
                // Sort longest first.
                return (cookie2->mPath.length() < cookie1->mPath.length()); 
            }
        };
        

        class CookieManager : public WTF::FastAllocBase
        {
        public:
            ///Constructor, Destructor
            CookieManager();
            explicit CookieManager(const CookieManagerParameters& params, WKAL::ResourceHandleManager* /*rhm*/);
            virtual ~CookieManager();
    
            bool Init();
            bool Shutdown();

            virtual bool OnHeadersRead(TransportInfo* tInfo);
            virtual bool OnHeadersSend(TransportInfo* tInfo );
            
            // This function is public now so that you can process the cookie header from inside an html call. BCCookieJarEA.cpp support.
            // The expected pHeaderValue is the cookie header value and doesn't including the leading "Set-Cookie: " part.
            void ProcessCookieHeader( const char8_t* pHeaderValue, const char8_t* pURI );

            FixedString8 GetCookieTextForURL(const WKAL::KURL& url, bool nameAndValueOnly = false);

            /// Deletes the cookies file as well as any existing session cookies. 
            void RemoveCookies();

            /// Use cases: 
            /// 1. Calling CookieManager->Shutdown(true) will result in removal of an existing cookie file and no cookie file to be saved.
            /// 2. Calling CookieManager->CompletelyRemoveCookies() and then CookieManager->Shutdown() would end up creating an empty Cookie File.
            /// Option 2 is usually not what the user wants. Option 1 is what the user is probably looking for.
            /// 3. Calling CookieManager->CompletelyRemoveCookies() would remove all the cookies information in the middle of a user session.
            /// This is pretty handy.

            /// Pass in an array of Cookie* with size specified by numCookies parameter. numCookies is changed inside the function to 
            /// the number of cookies read. It is also specified in terms of return value.
            uint16_t EnumerateCookies(Cookie** cookiePtrArray, uint16_t& numCookies);

			/// Pass in an array of Cookie* with size specified by numCookies parameter. The number of cookies read
			//  is specified in terms of return value.
			//TODO: Talk to Paul about the EnumerateCookies() function overload just above. That one modifies the input numCookies which seem
			//slightly dangerous to me. 
			uint16_t EnumerateCookies(char8_t** rawCookieData, uint16_t numCookiesToRead);

            const CookieManagerParameters& GetParameters() const;
            void  SetParametersAndInitialize(const CookieManagerParameters& parameters);

        protected:
            typedef eastl::list<Cookie*, EASTLAllocator> CookieList;

            CookieManager(const CookieManager& mgr);
            CookieManager& operator = (const CookieManager& mgr);
            
            /// Clear the stored cookies
            void ReadCookiesFromFile();
            void WriteCookiesToFile();
            /// Deletes the cookies file. It does not remove any existing session cookies.
            void DeleteCookiesFile();
            void ClearCookies();
            Cookie* ParseCookieHeader( const CookieFullTextFixedString8& headerValue, const char8_t* pURI = 0 );
            bool ValidateCookie( Cookie* cookie, const char8_t* pURI );
            
        protected:
#ifdef USE_EATHREAD_LIBRARY
            EA::Thread::Futex       mFutex;             // Thread safety.
#endif
            CookieManagerParameters mParams;            // 
            CookieList              mCookies;           // All cookies that we have.
            char8_t*                mCookieParseBuffer; // Holds an individual cookie for parsing. Corresponds to CookieManagerParameters::mMaxIndividualCookieSize
            bool                    mInitialized;       // 
        };
    }
}

#endif //BCCookieManagerEA_h
