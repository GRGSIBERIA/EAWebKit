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
// BCCookieManagerEA.cpp
// Written/Modified by Arpit Baldeva
//
// Implements cookies for EAWebkit as per RFC 2901, 2965 and original
// Netscape cookie specifications.
/////////////////////////////////////////////////////////////////////////////


#include "BCCookieManagerEA.h"

#include "BCDateEA.h"
#include "KURL.h"

#include <EAWebkit/EAWebKitTransport.h>
#include <EAWebKit/EAWebKitFileSystem.h>
#include <EAWebKit/internal/EAWebKitAssert.h>
#include <EAWebKit/internal/EAWebKitString.h>
#include <EASTL/sort.h>
#include <wtf/FastAllocBase.h>
#include <eastl/fixed_vector.h>

#ifndef _MSC_VER
    #define stricmp strcasecmp
#endif



///////////////////////////////////////////////////////////////////////////////
// Cookie attribute name constants
//
namespace EA
{
    namespace WebKit
    {
		char8_t* gEAWebKitCookieFormatVersion = "CookieFormat:1.1"; 

		char8_t* COOKIE_DELIMITER = "\n";
		
		namespace CookieManagerHelp
        {
            static const char8_t* kSetCookieHdrStr="Set-Cookie";
            static const char8_t* kSetCookieHdr2Str="Set-Cookie2";
            static const char8_t* kCookieHdrStr="Cookie";
            static const char8_t* kDomainStr = "domain";
            static const char8_t* kMaxAgeStr = "Max-Age";
            static const char8_t* kExpiresStr = "expires";
            static const char8_t* kPathStr = "path";
            static const char8_t* kCommentStr = "comment";
            static const char8_t* kCommentURLStr = "commenturl";
            static const char8_t* kVersionStr = "Version";
            static const char8_t* kSecureStr = "secure";
            static const char8_t* kPortStr = "port";

            char8_t* Strtok(char8_t* pString, const char8_t* pDelimiters, char8_t** pContext)
            {
                // find point on string to resume
                char8_t* s = pString;

                if(!s)
                {
                    s = *pContext;
                    if(!s)
                        return NULL;
                }

                // Compute bit hash based on lower 5 bits of delimiter characters
                const char8_t* d = pDelimiters;
                int32_t        hash = 0;
                uint32_t       delimiterCount = 0;

                while(const char8_t c = *d++)
                {
                    hash |= (0x80000000 >> (c & 31));
                    ++delimiterCount;
                }

                // Skip delimiters
                for(;;)
                {
                    const char8_t c = *s;

                    // If we hit the end of the string, it ends solely with delimiters
                    // and there are no more tokens to get.
                    if(!c)
                    {
                        *pContext = NULL;
                        return NULL;
                    }

                    // Fast rejection against hash set
                    if((hash << (c & 31)) >= 0)
                        break;

                    // brute-force search against delimiter list
                    for(uint32_t i=0; i<delimiterCount; ++i)
                    {
                        if (pDelimiters[i] == c)    // Is it a delimiter? ...
                            goto still_delimiters;  // yes, continue the loop
                    }

                    // Not a token, so exit
                    break;

                    still_delimiters:
                    ++s;
                }

                // Mark beginning of token
                char8_t* const pToken = s;

                // Search for end of token
                while(const char8_t c = *s)
                {
                    // Fast rejection against hash set
                    if((hash << (c & 31)) < 0)
                    {
                        // Brute-force search against delimiter list
                        for(uint32_t i=0; i<delimiterCount; ++i)
                        {
                            if(pDelimiters[i] == c)
                            {
                                // This token ends with a delimiter.
                                *s = 0;                 // null-term substring
                                *pContext = (s + 1);    // restart on next byte
                                return pToken;          // return found token
                            }
                        }
                    }

                    ++s;
                }

                // We found a token but it was at the end of the string, 
                // so we null out the context and return the last token.
                *pContext = NULL;           // no more tokens
                return pToken;              // return found token
            }

            
            bool EndsWithDomain( const FixedString8& server, const FixedString8& domain )
            {
                //The original code from UTFInternet package talked about implying a leading dot to the server name. I dont see it. Currently this code fails if we
                //type google.com in the browser address and it sends a cookie with domain .google.com. Lets put a leading dot if start is -1
                //and allow these domains.
                int32_t start  = server.length() - domain.length();
                if(start >= 0 )
                {
                    FixedString8 serversDomain = server.substr(start,eastl::string8::npos);
                    if( serversDomain.comparei(domain) == 0)
                    {
                        if( (start == 0) || (domain[0] == '.') || (server[start-1] == '.'))
                        {
                            return true;
                        }
                    }
                }
                else if (start == -1)
                {
                    FixedString8 serverWithDot(FixedString8::CtorSprintf(),".%s",server.c_str());
                    if( serverWithDot.comparei(domain) == 0)
                        return true;
                }
                return false;
            }

            ///////////////////////////////////////////////////////////////////////////////
            // Trim
            //
            // Trim whitespace from the begining and end of a string.  Modifies the
            // original string, and returns NULL if string is NULL or consists only 
            // of whitespace.
            //
            char8_t* Trim( char8_t* str )
            {
                if( NULL == str ) 
                    return NULL;
                size_t len = strlen( str );
                while( len > 0 && isspace( str[--len] ) )
                    str[len] = '\0';
                while( *str != '\0' && isspace( *str ) )
                    *str++ = '\0';
                return '\0' == *str ? NULL : str;
            }


            ///////////////////////////////////////////////////////////////////////////////
            // GetNameValue
            //
            // Parse a name/value pair separated by '=' from pairString, setting the name
            // and value pointers to the results. Whitespace is trimmed from ends of both and
            // if either name or value doesn't exist the pointer is set to NULL. Modifies
            // the original string while parsing.
            //
            void GetNameValue( char8_t* pairString, char8_t** name, char8_t** value )
            {
                *value = NULL;
                char8_t* end = pairString ? strchr( pairString, '=' ) : NULL;
                if( NULL != end )
                {
                    *end = NULL;
                    *value = Trim( end + 1 );
                }
                *name = Trim( pairString );
            }

            //////////////////////////////////////////////////////////////////////////
            // ValidateDomain
            //
            // Returns true if domain from offset onward is a valid cookie domain suffix.
            // A valid cookie domain suffix contains at least one '.' that is imbedded
            // e.g. 'a.b' but not '.aa' or '.a.'.  We don't check for '..a' as those
            // host names won't resolve and therefore aren't a liability.
            //
            static bool ValidateDomain( FixedString8& domain, FixedString8::size_type offset )
            {
                FixedString8::size_type len = domain.length();
                if( offset < FixedString8::npos && len > (offset + 1) )
                {
                    FixedString8::size_type dot = domain.find_first_of( '.', offset + 1 );
                    if( FixedString8::npos != dot && dot < (len-1) )
                        return true;
                }
                return false;

            }
        }


        CookieFullTextFixedString8 Cookie::CreateCookieString() const
        {
            using namespace CookieManagerHelp;

            char expirationTime[64];
            WriteDateString(mExpirationTime, expirationTime, sizeof(expirationTime));

            CookieFullTextFixedString8 cookieStr(CookieFullTextFixedString8::CtorSprintf(),
                "%s=%s; %s=%s; %s=%s; %s=%s",
                mName.c_str(),mValue.c_str(),
                kPathStr, mPath.c_str(),
                kDomainStr, mDomain.c_str(),
                kExpiresStr, expirationTime) ;
            
            if(mSecure)
                cookieStr.append_sprintf("; %s",kSecureStr);
            
            if(!mComment.empty())
                cookieStr.append_sprintf("; %s=%s",kCommentStr,mComment.c_str());
            
            if(!mCommentURL.empty())
                cookieStr.append_sprintf("; %s=%s",kCommentURLStr,mCommentURL.c_str());
            
            if(mPorts[0])
            {
                cookieStr.append_sprintf("; %s=\"",kPortStr);
                for(int portIndex = 0; portIndex< Cookie::MAX_NUM_PORTS && mPorts[portIndex]; ++portIndex)
                {
                    char8_t buffer[32];
                    sprintf(buffer,"%u",mPorts[portIndex]);
                    cookieStr.append_sprintf("%s,",buffer);
                }
                cookieStr.erase(cookieStr.length()-1); //erase the last comma
                cookieStr.append_sprintf("\"");
            }

            return cookieStr;
        }

        bool Cookie::IsCookieExpired(time_t timeNow) const
        {
            return (timeNow >= mExpirationTime); // >= in case the server sends a 0 delta tick time.
        }

        CookieManager::CookieManager()
            : mParams()
            , mCookieParseBuffer(NULL)
            , mInitialized(false)
        {
        }

        CookieManager::CookieManager(const CookieManagerParameters& params, WKAL::ResourceHandleManager* /*rhm*/)
            : mParams(params)
            , mCookieParseBuffer(NULL)
            , mInitialized(false)
        { 
             
        }

        CookieManager::~CookieManager()
        {
            Shutdown();  // In case the user hasn't called it.
        }

        bool CookieManager::Init()
        {
            if(!mInitialized)
            {
                mInitialized = true;
				
				//Make sure that you don't allocate the memory if it is already allocated in the SetParameters(). This may happen if the user calls the SetCookieUsage multiple times.
				//Failure to put this check can cause a subtle leak if the max user cookie size is not the same as the default one.
				if(!mCookieParseBuffer)
					mCookieParseBuffer = WTF::fastNewArray<char8_t>(mParams.mMaxIndividualCookieSize);
                
				ReadCookiesFromFile();
            }
            return true;
        }

        bool CookieManager::Shutdown()
        {
            if(mInitialized)
            {
                mInitialized = false;

                WTF::fastDeleteArray<char8_t> (mCookieParseBuffer);
                mCookieParseBuffer = 0;

                if(mParams.mMaxCookieCount)
                    WriteCookiesToFile();
                else
                    RemoveCookies();
                
                ClearCookies();
            }
            return true;
        }

        void CookieManager::ClearCookies()
        {
#ifdef USE_EATHREAD_LIBRARY
            EA::Thread::AutoFutex futex(mFutex);
#endif            
            for(CookieList::const_iterator iter = mCookies.begin(); iter != mCookies.end(); ++iter)
                delete *iter;
            
			mCookies.clear();
        }

        void CookieManager::DeleteCookiesFile()
        {
            if(!mParams.mCookieFilePath.empty())
            {
                EA::WebKit::FileSystem* fileSystem = EA::WebKit::GetFileSystem();
                fileSystem->RemoveFile(mParams.mCookieFilePath.c_str());
            }
        }

        void CookieManager::RemoveCookies()
        {
            DeleteCookiesFile();
            ClearCookies();
        }

        void CookieManager::ReadCookiesFromFile()
        {
            using namespace CookieManagerHelp;
#ifdef USE_EATHREAD_LIBRARY
            EA::Thread::AutoFutex futex(mFutex);
#endif            
            if(!mParams.mCookieFilePath.empty())
            {
                //Parse and cache the cookies in the memory
                EA::WebKit::FileSystem* fileSystem = EA::WebKit::GetFileSystem();
                EA::WebKit::FileSystem::FileObject fileObject = fileSystem->CreateFileObject();
                if(fileSystem->OpenFile(fileObject, mParams.mCookieFilePath.c_str(), FileSystem::kRead))
                {
                    int64_t numBytes;
                    fileSystem->GetFileSize(mParams.mCookieFilePath.c_str(),numBytes);
                    if(numBytes>0) //Have something to read
                    {
                        char* pCookieFileBuffer = WTF::fastNewArray<char8_t> (numBytes+1);  // +1 so we can put a \0 at the end.
                        fileSystem->ReadFile(fileObject, pCookieFileBuffer, numBytes);
                        fileSystem->CloseFile(fileObject);
                        pCookieFileBuffer[numBytes] = '\0';

                        char8_t* ctx = NULL;
						char8_t* cookieFormatVersion = NULL;
						char8_t* cookieHeader = NULL;
                        Cookie* cookie = NULL;
                        //Create Cookies from the cookie file buffer here
                        //First line is the header line which contains only version info for now.
						cookieFormatVersion = Strtok( pCookieFileBuffer, COOKIE_DELIMITER, &ctx );
						//read cookies only if the format matches and the delimiter was found.
						if(cookieFormatVersion && EA::Internal::Stricmp(cookieFormatVersion, gEAWebKitCookieFormatVersion) == 0)
						{
							//The file format is really simple. You have 1 cookie per line. The cookies are separated from each other by
							//a new line character.
							while( NULL != (cookieHeader = Strtok( NULL, COOKIE_DELIMITER, &ctx )) )
							{
								cookie = ParseCookieHeader(cookieHeader);
								EAW_ASSERT_MSG(cookie, "Looks like the cookie is corrupted");
								if(cookie) //Add an extra check to make sure that the pointer is valid
									mCookies.push_back(cookie);
							}

						}
						
                        WTF::fastDeleteArray<char8_t> (pCookieFileBuffer);
                        pCookieFileBuffer = NULL;
                    }
                }
                fileSystem->DestroyFileObject(fileObject);  // 2/25/09 CSidhall - Moved up one bracket for minor leak fix
            }
        }

        void CookieManager::WriteCookiesToFile()
        {
            using namespace CookieManagerHelp;
#ifdef USE_EATHREAD_LIBRARY
            EA::Thread::AutoFutex futex(mFutex);
#endif            
            if(!mParams.mCookieFilePath.empty())
            {
                EA::WebKit::FileSystem* fileSystem = EA::WebKit::GetFileSystem();
                EA::WebKit::FileSystem::FileObject fileObject = fileSystem->CreateFileObject();
                if(fileSystem->OpenFile(fileObject, mParams.mCookieFilePath.c_str(), FileSystem::kWrite))
                {
                    uint32_t fileSizeCurrent = 0;
					
					fileSystem->WriteFile(fileObject,gEAWebKitCookieFormatVersion, EA::Internal::Strlen(gEAWebKitCookieFormatVersion));
					fileSystem->WriteFile(fileObject, COOKIE_DELIMITER, 1);//strlen(COOKIE_DELIMITER) = 1
					fileSizeCurrent += (EA::Internal::Strlen(gEAWebKitCookieFormatVersion) + 1);////strlen(COOKIE_DELIMITER) = 1
					
					time_t timeNow = ::time(NULL);

                    // To consider: Write the cookies in order from newest to oldest received, so that if 
                    // we have to purge some of them due to lack of space then we lose just the oldest ones.

                    for(CookieList::const_iterator iter = mCookies.begin(); iter != mCookies.end() && (fileSizeCurrent < mParams.mMaxCookieFileSize); ++iter)
                    {
                        const Cookie* cookie = *iter;
                        //Don't write these cookies to the file. Not intended to be saved.
                        if(cookie->IsCookieExpired(timeNow) || cookie->mExpiresAtEndOfSession) 
                            continue;

                        CookieFullTextFixedString8 cookieText = cookie->CreateCookieString();

						fileSystem->WriteFile(fileObject,cookieText.c_str(),cookieText.length());
                        fileSystem->WriteFile(fileObject, COOKIE_DELIMITER, 1);//strlen(COOKIE_DELIMITER) = 1
                        fileSizeCurrent += (cookieText.length() + 1); ////strlen(COOKIE_DELIMITER) = 1
                    }
                    fileSystem->CloseFile(fileObject);
                }
                fileSystem->DestroyFileObject(fileObject);
            }
        }

        const CookieManagerParameters& CookieManager::GetParameters() const
        {
            return mParams;
        }

        void CookieManager::SetParametersAndInitialize(const CookieManagerParameters& parameters)
        {
            if(parameters.mMaxCookieCount == 0) // If we are disabling cookies...
			{
				RemoveCookies();
				return;
			}

            if(parameters.mMaxCookieCount < mCookies.size())
            {
                // This is a crude way to reduce the cookie count. Would be better if we chose based on dates.
                mCookies.resize(parameters.mMaxCookieCount);
            }

            if(parameters.mMaxIndividualCookieSize != mParams.mMaxIndividualCookieSize)
            {
                if(mCookieParseBuffer)
                    WTF::fastDeleteArray<char8_t>(mCookieParseBuffer);
                mCookieParseBuffer = WTF::fastNewArray<char8_t>(parameters.mMaxIndividualCookieSize);
            }

            // parameters.mMaxCookieFileSize is only used when we write cookies on shutdown.

            mParams = parameters;
			
			EAW_ASSERT_FORMATTED(parameters.mMaxCookieCount != 0, "%s", "CookieManager: No cookies!\n");
			
			//If we are already initialized, the user wants to change the way it uses cookies. So we shutdown our manager and reinitialize it 
			//with the new user parameters. I don't see the user change their cookie usage at runtime but if they do, we cover it.
			if(mInitialized)
			{
				Shutdown();
				Init();
			}
			else //We are not initialized. Initialize the manager.
			{
				Init();
			}
        }

        // This is called anytime the ResourceHandleManager has read the data. This means that
        // the headers containing the cookie information are downloaded from an http response.
        bool CookieManager::OnHeadersRead(TransportInfo* tInfo)
        {
            using namespace CookieManagerHelp;
#ifdef USE_EATHREAD_LIBRARY
            EA::Thread::AutoFutex futex( mFutex );
#endif
            if(mParams.mMaxCookieCount == 0)
                return false;
			
			if(!mInitialized)
			{
				CookieManagerParameters    params;
				SetParametersAndInitialize(params);
			}

            FixedString8 headerKey, headerValue, uri;
            EA::WebKit::ConvertToString8(*GET_FIXEDSTRING16(tInfo->mEffectiveURI), uri);    
            
            //Need to iterate through all the headers....there may be multiple Set-Cookie(2) header lines.
            for(HeaderMap::const_iterator it= GET_HEADERMAP(tInfo->mHeaderMapIn)->begin();it!= GET_HEADERMAP(tInfo->mHeaderMapIn)->end();++it)
            {
                EA::WebKit::ConvertToString8(it->first, headerKey);
                
                if(headerKey.comparei(kSetCookieHdrStr) == 0 || headerKey.comparei(kSetCookieHdr2Str) == 0)
                {
                    EA::WebKit::ConvertToString8(it->second, headerValue);
                    ProcessCookieHeader(headerValue.c_str(),uri.c_str());
                }
            }
            return true;
        }

        // This is called anytime the ResourceHandleManager is about to send data out. This means that
        // the headers containing the cookie information are ready to be sent as part of a http request.
        bool CookieManager::OnHeadersSend(TransportInfo* tInfo )
        {
            using namespace CookieManagerHelp;

            if(mParams.mMaxCookieCount == 0)
                return false;
			
			if(!mInitialized)
			{
				CookieManagerParameters    params;
				SetParametersAndInitialize(params);
			}

            FixedString8 url8; 
            EA::WebKit::ConvertToString8(*GET_FIXEDSTRING16(tInfo->mEffectiveURI),url8);
            OWBAL::KURL url(url8.c_str());

            FixedString8 cookieString = GetCookieTextForURL(url);
            
            if(!cookieString.empty())
            {
                FixedString8 cookieHdr8 = kCookieHdrStr;
                FixedString16 cookieHdr; 
                EA::WebKit::ConvertToString16(cookieHdr8,cookieHdr);
            
                FixedString16 cookieVal; 
                EA::WebKit::ConvertToString16(cookieString,cookieVal);

                EA::WebKit::HeaderMap::value_type eaValue(cookieHdr, cookieVal);
                GET_HEADERMAP(tInfo->mHeaderMapOut)->insert(eaValue);
            }    
            return true;
        }

        FixedString8 CookieManager::GetCookieTextForURL(const WKAL::KURL& url, bool nameAndValueOnly)
        {
            using namespace CookieManagerHelp;

#ifdef USE_EATHREAD_LIBRARY
            EA::Thread::AutoFutex futex( mFutex );
#endif
            FixedString8 cookieString;
           
            if(mParams.mMaxCookieCount == 0 || !mInitialized)
                return cookieString; //Return Early

            FixedString16 hostStr16;
            hostStr16.assign(url.host().characters(),url.host().length());
            FixedString8 host; 
            EA::WebKit::ConvertToString8(hostStr16,host);

            FixedString16 pathStr16;
            pathStr16.assign(url.path().characters(),url.path().length());
            FixedString8 path; 
            EA::WebKit::ConvertToString8(pathStr16,path);

            FixedString16 schemeStr16;
            schemeStr16.assign(url.protocol().characters(),url.protocol().length());
            FixedString8 scheme; 
            EA::WebKit::ConvertToString8(schemeStr16,scheme);

            time_t timeNow = ::time(NULL);
            
            if( !host.empty() && !path.empty() )
            {
                int lowestVersion = 1;

                //AJBTODO: 32 is probably a bit high for our case
                typedef eastl::fixed_vector<const Cookie*, 32, true, EASTLAllocator> LocalMatches;
                LocalMatches matches;

                for( CookieList::const_iterator it = mCookies.begin(); it != mCookies.end(); ++it )
                {
                    const Cookie* cookie = *it;

                    //Match the domain first. 
                    if(!EndsWithDomain(host, cookie->mDomain))
                        continue;

                    //Now match the path
                    if(path.find(cookie->mPath) != 0)
                        continue;

                    //Check if the cookie expired
                    if(cookie->IsCookieExpired(timeNow))
                        continue;

                    //Check if the cookie is secure and if it is, what is the connection type
                    if(cookie->mSecure && scheme.comparei("https") != 0)
                        continue;

                    //Check if the cookie is being sent to a valid port
                    if(cookie->mPorts[0])
                    {
                        uint16_t urlPort(80);
                        if(url.port())
                            urlPort = url.port();
                        uint16_t index = 0;
                        for(; index < Cookie::MAX_NUM_PORTS ; ++index)
                        {
                            if(cookie->mPorts[index] == urlPort)
                                break;
                        }
                        if(index == Cookie::MAX_NUM_PORTS)
                            continue;
                    }

                    //Congratulations! Cookies passed all the criteria.

                    // So figure out the lowest version cookie for the cookies matching with this server. If the server did not specify a version
                    //for the cookie, this would result in the lowestVersion being set to 0.
                    if( lowestVersion > cookie->mVersion )
                        lowestVersion = cookie->mVersion;

                    matches.push_back(cookie);
                }

                if( !matches.empty() )
                {
                    //sort by path length for specificity
                    eastl::sort(matches.begin(),matches.end(),CookiePathSorter());

                    //Only send the version string if we received one at the first place.
                    //Not sure what number to send in case of mixed versions. Sending the lowest at the moment.
                    if( lowestVersion > 0 && !nameAndValueOnly)
                        cookieString.sprintf( "$Version=%d; ", lowestVersion );

                    bool firstCookie = true;
					for( LocalMatches::const_iterator cit = matches.begin(); matches.end() != cit; ++cit)
                    {
                        const Cookie* cookie = *cit;
                        
						if(!firstCookie)
							cookieString.append(";");
						else
							firstCookie = false;

						cookieString.append( cookie->mName.c_str() ).append( "=" ).append( cookie->mValue.c_str() );

                        if(lowestVersion > 0 && !nameAndValueOnly) //Send the Domain and Path string if the version is greater than 0.
                        {
							if(!cookie->mPathDefaulted )
								cookieString.append( "; $Path=" ).append( cookie->mPath.c_str() );
							
							if(!cookie->mDomainDefaulted )
                                cookieString.append( "; $Domain=" ).append( cookie->mDomain.c_str() );

                            
                            if(cookie->mPorts[0]) //so there is a port, append all the ports in the portlist
                            {
                                cookieString.append( "; $Port=\"" );
                                for(int portIndex = 0; portIndex< Cookie::MAX_NUM_PORTS && cookie->mPorts[portIndex]; ++portIndex)
                                {
                                    char8_t buffer[32];
                                    sprintf(buffer,"%u,",cookie->mPorts[portIndex]);
                                    cookieString.append(buffer);
                                }
                                cookieString.pop_back(); //erase the last comma
                                cookieString.append( "\"" );//append the "
                            }
                        }
                    }
                }
            }

            return cookieString;
        }

        uint16_t CookieManager::EnumerateCookies(Cookie** cookiePtrArray, uint16_t& numCookies)
        {
#ifdef USE_EATHREAD_LIBRARY
            EA::Thread::AutoFutex futex(mFutex);
#endif
            if(numCookies > mCookies.size())
                numCookies = mCookies.size();

            int index = 0;

            for(CookieList::const_iterator iter = mCookies.begin(); (iter != mCookies.end() && index<numCookies); ++iter, ++index)
                cookiePtrArray[index] = *iter;

            return numCookies;
        }

        
		uint16_t CookieManager::EnumerateCookies(char8_t** rawCookieData, uint16_t numCookiesToRead)
		{
#ifdef USE_EATHREAD_LIBRARY
			EA::Thread::AutoFutex futex(mFutex);
#endif
			int index = 0;

			for(CookieList::const_iterator iter = mCookies.begin(); (iter != mCookies.end() && index< numCookiesToRead) ; ++iter, ++index)
				strcpy(rawCookieData[index],(*iter)->CreateCookieString().c_str());

			return index;
		}

		void CookieManager::ProcessCookieHeader( const char8_t* pHeaderValue, const char8_t* pURI )
        {
            if(mParams.mMaxCookieCount == 0 || !mInitialized)
                return;

            Cookie* cookie = ParseCookieHeader( pHeaderValue, pURI );
            time_t timeNow = ::time(NULL);

            if( cookie && ValidateCookie( cookie, pURI ) )
            {
                // see if this cookie is replacing or removing an existing one
                for( CookieList::iterator it = mCookies.begin(); it != mCookies.end(); it++ )
                {
                    Cookie* c = *it;
                    if( 0 == c->mName.compare( cookie->mName ) && 0 == c->mDomain.comparei( cookie->mDomain )
                        && 0 == c->mPath.compare( cookie->mPath ) )
                    {
                        //Found an existing cookie to match the new cookie. Delete existing cookie. 
                        delete *it;
                        //An existing cookie has to come from a server. 
                        //1. If the server does not set the expire/max-age attribute, it is a session cookie. Just replace the existing one.
                        //2. If the server does set the expire/max-age attribute, check if the cookie has expired (Servers do it by setting 0 as max-age
                        // or by setting a date in the past). If yes, remove it.
                        
                        if(cookie->IsCookieExpired(timeNow) && !cookie->mExpiresAtEndOfSession)
                        {
                                delete cookie;
                                mCookies.erase( it );
                        }
                        else //Just overwrite the existing cookie
                        {
                            *it = cookie;
                        }

                        return; // stop processing we are done
                    }
                }

                //So you have a new cookie now. It can come from a server or you may be reading it from the file. 
                //1. If the cookie coming from the file, mExpiresAtEndOfSession is always going to be false. So if cookie is expired, remove it. If not,
                // add it to the list.
                //2. If the cookie is coming from server with an expires/max-age attribute set, mExpiresAtEndOfSession is always going to be false. So if
                //the cookie is expired, remove it. If not, add it to the list.
                //3. If the cookie is coming from server without an expire attribute, it is a session cookie and mExpiresAtEndOfSession is going to be true.
                // In that case, you have to add the cookie to the existing list.

                if(cookie->IsCookieExpired(timeNow) && !cookie->mExpiresAtEndOfSession)
                {
                    delete cookie;
                }
                else
                {
                    // add a new cookie
                    mCookies.push_back( cookie );

                    // enforce the maximum number of cookies
                    //AJBTODO: We can code something like delete least frequently used cookie here rather than the oldest cookie.
                    if( mCookies.size() > mParams.mMaxCookieCount )
                        mCookies.pop_front();
                }
                
            }
			else if(cookie) //could not validate cookie.
			{
				delete cookie;
			}

        }

        Cookie* CookieManager::ParseCookieHeader( const CookieFullTextFixedString8& headerValue, const char8_t* pURI )
        {
            using namespace CookieManagerHelp;

            char8_t *ctx=NULL;
			char8_t *name, *value, *token;
            
            if(headerValue.length() >= mParams.mMaxIndividualCookieSize)
            {
                EAW_ASSERT_FORMATTED(false, "CookieManager: The header is too long for the cookie buffer. Required:%d,Available:%d\n", headerValue.length(), mParams.mMaxIndividualCookieSize);
                return NULL;
            }

            // copy header string into a buffer to destructively parse in place avoiding copying
            strcpy( mCookieParseBuffer, headerValue.c_str() );
            // first token pair is the cookie name/value
            GetNameValue( Strtok( mCookieParseBuffer, ";", &ctx ), &name, &value );

            if( NULL == name || name[0] == '$' )
                return NULL; // invalid name, don't bother creating cookie

            Cookie* cookie = new Cookie(name);
            //cookie->mFullCookieText = headerValue.c_str();
            if( NULL != value )
                cookie->mValue = value;

            // parse the cookie attributes
            while( NULL != (token = Strtok( NULL, ";", &ctx )) )
            {
                GetNameValue( token, &name, &value);
                if( NULL != name )
                {
                    if( 0 == stricmp( name, kDomainStr ) )
                    {
                        if( NULL != value && strlen( value ) > 2 )
                        {
                            cookie->mDomain = value;
                            cookie->mDomainDefaulted = false;
                        }

                    }
                    else if( 0 == stricmp( name, kMaxAgeStr ) )
                    {
						//	http://www.ietf.org/rfc/rfc2109.txt
						//	Max-Age=delta-seconds
						//	Optional.  The Max-Age attribute defines the lifetime of the
						//	cookie, in seconds.  The delta-seconds value is a decimal non-
						//	negative integer.  After delta-seconds seconds elapse, the client
						//	should discard the cookie.  A value of zero means the cookie
						//	should be discarded immediately.
						if(NULL != value)
                        {
                            uint32_t deltaTime;
                            sscanf(value,"%u",&deltaTime);
                            cookie->mExpirationTime = ::time(NULL) + deltaTime;
                        }                        

                        cookie->mExpiresAtEndOfSession = false;
                    }
                    else if( 0 == stricmp( name, kExpiresStr ) )
                    {
                        if(NULL != value)
                            cookie->mExpirationTime = ReadDateString(value);

                        cookie->mExpiresAtEndOfSession = false;
                    }
                    else if( 0 == stricmp( name, kPathStr ) )
                    {
                        if( NULL != value && strlen( value ) > 0 )
                        {
                            cookie->mPathDefaulted = false;
                            cookie->mPath = value;
                        }
                    }
                    else if( 0 == stricmp( name, kCommentStr ) )
                    {
                        if(NULL != value)
                            cookie->mComment = value;
                    }
                    else if( 0 == stricmp( name, kCommentURLStr ) )
                    {
                        if(NULL != value)
                            cookie->mCommentURL = value;
                    }
                    else if( 0 == stricmp( name, kVersionStr ) )
                    {
                        if( NULL != value)
                            sscanf(value,"%hu",&(cookie->mVersion));
                    }
                    else if( 0 == stricmp( name, kSecureStr ) )
                    {
                        cookie->mSecure = true;
                    }
                    else if( 0 == stricmp( name, kPortStr ) )
                    {
                        if( NULL != value)
                        {
                            eastl::string8 portsStr(value);
                            //Since we know that there is definitely going to be just 1 leading and trailing \", we can do it as follows.
                            portsStr.erase(0,1);
                            portsStr.erase(portsStr.length()-1);
                            
                            const size_t portsBufferLength = 255;
                            char ports[portsBufferLength+1];
                            if(portsStr.length() <= portsBufferLength)
                            {
                                strcpy(ports,portsStr.c_str());
                                char* ctx2 =NULL;
                                char* port =NULL;
                                port = Strtok( ports, ",", &ctx2 );
                                if(port)
                                {
                                    int portIndex = 0;
                                    sscanf(port,"%u",&(cookie->mPorts[portIndex++]));
                                    
                                    while( NULL != (port = Strtok( NULL, ",", &ctx2 )) )
                                    {
                                        if(portIndex < (Cookie::MAX_NUM_PORTS - 1))
                                            sscanf(port,"%u",&(cookie->mPorts[portIndex++]));
                                    }
                                    cookie->mPorts[portIndex] = 0; //This means the end of the ports
                                }
                            }
                            else
                            {
                                EAW_ASSERT_FORMATTED(false, "CookieManager: Not enough buffer for ports. Required:%d, Available:%d\n", portsStr.length(), portsBufferLength);
                            }

                        }
                        else //Else no ports are specified specifically. This means accept *only* the current port.
                        {
                            uint32_t currentPort(80);
                            OWBAL::KURL url(pURI);
                            if(url.port())
                            {
                                currentPort = url.port();
                            }
                            cookie->mPorts[0] = currentPort;
                            cookie->mPorts[1] = 0;
                        }
                    }

                }
            }

            return cookie;
            
        }

        bool CookieManager::ValidateCookie( Cookie* cookie, const char8_t* pURI )
        {
            using namespace CookieManagerHelp;

            OWBAL::KURL url(pURI);

            //make sure that domain is valid
            FixedString16 hostStr16;
            hostStr16.assign(url.host().characters(),url.host().length());
            FixedString8 host; 
            EA::WebKit::ConvertToString8(hostStr16, host);

            if( cookie->mDomainDefaulted )
            {
                // default domain is full request URI server name
                cookie->mDomain = host;
                // if the server has a valid domain suffix, use that instead
                FixedString8::size_type dot = cookie->mDomain.find( '.' );
                if( ValidateDomain( cookie->mDomain, dot ) )
                    cookie->mDomain = cookie->mDomain.substr( dot, FixedString8::npos);
            }
            else
            {
                if( ValidateDomain( cookie->mDomain, 0 ) )       
                {
                    if( !EndsWithDomain( host, cookie->mDomain ) )
                        return false;
                }
                else if( 0 != cookie->mDomain.comparei( host ) )
                {
                    // the full server name is valid as a cookie domain also and will fail
                    // domain validation in cases like local hosts without a '.' in the name 
                    // or servers of the form 'blah.com', so we pass through cases where the 
                    // full string compare succeeds above
                    return false;
                }
            }

            //make sure that path is valid

            FixedString16 pathStr16;
            pathStr16.assign(url.path().characters(),url.path().length());
            FixedString8 path; 
            EA::WebKit::ConvertToString8(pathStr16,path);
                    
            if( cookie->mPathDefaulted )
            {
                // default path is full URI path
                cookie->mPath = path; 
                cookie->mPath = cookie->mPath.substr( 0, cookie->mPath.find_last_of( '/' ) ); // substr handles npos for n
            }
            else
            {
                // validate that the request URI path is prefixed by cookie path attribute
                if( 0 != path.find( cookie->mPath ) )
                    return false;
            }

            //Make sure that the port list has the port on which the request was made
            uint32_t urlPort = url.port();
            
            //urlport is 0
            if(!urlPort) 
                urlPort = 80;
            
            if(!cookie->IsValidPort(urlPort))
                return false;


            return true;
        }

    }

}





