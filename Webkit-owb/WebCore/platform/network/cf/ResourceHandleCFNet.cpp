/*
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#include "config.h"

#include "ResourceHandle.h"
#include "ResourceHandleClient.h"
#include "ResourceHandleInternal.h"

#include "AuthenticationCF.h"
#include "AuthenticationChallenge.h"
#include "CookieStorageWin.h"
#include "CString.h"
#include "DocLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "Logging.h"
#include "NotImplemented.h"
#include "ResourceError.h"
#include "ResourceResponse.h"

#include <WTF/HashMap.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <process.h> // for _beginthread()

#include <CFNetwork/CFNetwork.h>
#include <WebKitSystemInterface/WebKitSystemInterface.h>

namespace WebCore {

static HashSet<String>& allowsAnyHTTPSCertificateHosts()
{
    static HashSet<String> hosts;

    return hosts;
}

static HashMap<String, RetainPtr<CFDataRef> >& clientCerts()
{
    static HashMap<String, RetainPtr<CFDataRef> > certs;
    return certs;
}

CFURLRequestRef willSendRequest(CFURLConnectionRef conn, CFURLRequestRef cfRequest, CFURLResponseRef cfRedirectResponse, const void* clientInfo)
{
    ResourceHandle* handle = (ResourceHandle*)clientInfo;

    if (!cfRedirectResponse) {
        CFRetain(cfRequest);
        return cfRequest;
    }

    LOG(Network, "CFNet - willSendRequest(conn=%p, handle=%p) (%s)", conn, handle, handle->request().url().string().utf8().data());

    ResourceRequest request(cfRequest);
    if (handle->client())
        handle->client()->willSendRequest(handle, request, cfRedirectResponse);

    cfRequest = request.cfURLRequest();

    CFRetain(cfRequest);
    return cfRequest;
}

void didReceiveResponse(CFURLConnectionRef conn, CFURLResponseRef cfResponse, const void* clientInfo) 
{
    ResourceHandle* handle = (ResourceHandle*)clientInfo;

    LOG(Network, "CFNet - didReceiveResponse(conn=%p, handle=%p) (%s)", conn, handle, handle->request().url().string().utf8().data());

    if (handle->client())
        handle->client()->didReceiveResponse(handle, cfResponse);
}

void didReceiveData(CFURLConnectionRef conn, CFDataRef data, CFIndex originalLength, const void* clientInfo) 
{
    ResourceHandle* handle = (ResourceHandle*)clientInfo;
    const UInt8* bytes = CFDataGetBytePtr(data);
    CFIndex length = CFDataGetLength(data);

    LOG(Network, "CFNet - didReceiveData(conn=%p, handle=%p, bytes=%d) (%s)", conn, handle, length, handle->request().url().string().utf8().data());

    if (handle->client())
        handle->client()->didReceiveData(handle, (const char*)bytes, length, originalLength);
}

void didFinishLoading(CFURLConnectionRef conn, const void* clientInfo) 
{
    ResourceHandle* handle = (ResourceHandle*)clientInfo;

    LOG(Network, "CFNet - didFinishLoading(conn=%p, handle=%p) (%s)", conn, handle, handle->request().url().string().utf8().data());

    if (handle->client())
        handle->client()->didFinishLoading(handle);
}

void didFail(CFURLConnectionRef conn, CFErrorRef error, const void* clientInfo) 
{
    ResourceHandle* handle = (ResourceHandle*)clientInfo;

    LOG(Network, "CFNet - didFail(conn=%p, handle=%p, error = %p) (%s)", conn, handle, error, handle->request().url().string().utf8().data());

    if (handle->client())
        handle->client()->didFail(handle, ResourceError(error));
}

CFCachedURLResponseRef willCacheResponse(CFURLConnectionRef conn, CFCachedURLResponseRef cachedResponse, const void* clientInfo) 
{
    ResourceHandle* handle = (ResourceHandle*)clientInfo;

    CacheStoragePolicy policy = static_cast<CacheStoragePolicy>(CFCachedURLResponseGetStoragePolicy(cachedResponse));

    if (handle->client())
        handle->client()->willCacheResponse(handle, policy);

    if (static_cast<CFURLCacheStoragePolicy>(policy) != CFCachedURLResponseGetStoragePolicy(cachedResponse))
        cachedResponse = CFCachedURLResponseCreateWithUserInfo(kCFAllocatorDefault, 
                                                               CFCachedURLResponseGetWrappedResponse(cachedResponse),
                                                               CFCachedURLResponseGetReceiverData(cachedResponse),
                                                               CFCachedURLResponseGetUserInfo(cachedResponse), 
                                                               static_cast<CFURLCacheStoragePolicy>(policy));
    CFRetain(cachedResponse);

    return cachedResponse;
}

void didReceiveChallenge(CFURLConnectionRef conn, CFURLAuthChallengeRef challenge, const void* clientInfo)
{
    ResourceHandle* handle = (ResourceHandle*)clientInfo;
    ASSERT(handle);
    LOG(Network, "CFNet - didReceiveChallenge(conn=%p, handle=%p (%s)", conn, handle, handle->request().url().string().utf8().data());

    handle->didReceiveAuthenticationChallenge(AuthenticationChallenge(challenge, handle));
}

void addHeadersFromHashMap(CFMutableURLRequestRef request, const HTTPHeaderMap& requestHeaders) 
{
    if (!requestHeaders.size())
        return;

    HTTPHeaderMap::const_iterator end = requestHeaders.end();
    for (HTTPHeaderMap::const_iterator it = requestHeaders.begin(); it != end; ++it) {
        CFStringRef key = it->first.createCFString();
        CFStringRef value = it->second.createCFString();
        CFURLRequestSetHTTPHeaderFieldValue(request, key, value);
        CFRelease(key);
        CFRelease(value);
    }
}

ResourceHandleInternal::~ResourceHandleInternal()
{
    if (m_connection) {
        LOG(Network, "CFNet - Cancelling connection %p (%s)", m_connection, m_request.url().string().utf8().data());
        CFURLConnectionCancel(m_connection.get());
    }
}

ResourceHandle::~ResourceHandle()
{
    LOG(Network, "CFNet - Destroying job %p (%s)", this, d->m_request.url().string().utf8().data());
}

CFArrayRef arrayFromFormData(const FormData& d)
{
    size_t size = d.elements().size();
    CFMutableArrayRef a = CFArrayCreateMutable(0, d.elements().size(), &kCFTypeArrayCallBacks);
    for (size_t i = 0; i < size; ++i) {
        const FormDataElement& e = d.elements()[i];
        if (e.m_type == FormDataElement::data) {
            CFDataRef data = CFDataCreate(0, (const UInt8*)e.m_data.data(), e.m_data.size());
            CFArrayAppendValue(a, data);
            CFRelease(data);
        } else {
            ASSERT(e.m_type == FormDataElement::encodedFile);
            CFStringRef filename = e.m_filename.createCFString();
            CFArrayAppendValue(a, filename);
            CFRelease(filename);
        }
    }
    return a;
}

void emptyPerform(void* unused) 
{
}

static CFRunLoopRef loaderRL = 0;
void runLoaderThread(void *unused)
{
    loaderRL = CFRunLoopGetCurrent();

    // Must add a source to the run loop to prevent CFRunLoopRun() from exiting
    CFRunLoopSourceContext ctxt = {0, (void *)1 /*must be non-NULL*/, 0, 0, 0, 0, 0, 0, 0, emptyPerform};
    CFRunLoopSourceRef bogusSource = CFRunLoopSourceCreate(0, 0, &ctxt);
    CFRunLoopAddSource(loaderRL, bogusSource,kCFRunLoopDefaultMode);

    CFRunLoopRun();
}

CFRunLoopRef ResourceHandle::loaderRunLoop()
{
    if (!loaderRL) {
        _beginthread(runLoaderThread, 0, 0);
        while (loaderRL == 0) {
            // FIXME: sleep 10? that can't be right...
            Sleep(10);
        }
    }
    return loaderRL;
}

static CFURLRequestRef makeFinalRequest(const ResourceRequest& request, bool shouldContentSniff)
{
    CFMutableURLRequestRef newRequest = CFURLRequestCreateMutableCopy(kCFAllocatorDefault, request.cfURLRequest());
    
    if (!shouldContentSniff)
        wkSetCFURLRequestShouldContentSniff(newRequest, false);

    RetainPtr<CFMutableDictionaryRef> sslProps;

    if (allowsAnyHTTPSCertificateHosts().contains(request.url().host().lower())) {
        sslProps.adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
        CFDictionaryAddValue(sslProps.get(), kCFStreamSSLAllowsAnyRoot, kCFBooleanTrue);
        CFDictionaryAddValue(sslProps.get(), kCFStreamSSLAllowsExpiredRoots, kCFBooleanTrue);
    }

    HashMap<String, RetainPtr<CFDataRef> >::iterator clientCert = clientCerts().find(request.url().host().lower());
    if (clientCert != clientCerts().end()) {
        if (!sslProps)
            sslProps.adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
        wkSetClientCertificateInSSLProperties(sslProps.get(), (clientCert->second).get());
    }

    if (sslProps)
        CFURLRequestSetSSLProperties(newRequest, sslProps.get());

    if (CFHTTPCookieStorageRef cookieStorage = currentCookieStorage()) {
        CFURLRequestSetHTTPCookieStorage(newRequest, cookieStorage);
        CFURLRequestSetHTTPCookieStorageAcceptPolicy(newRequest, CFHTTPCookieStorageGetCookieAcceptPolicy(cookieStorage));
    }

    return newRequest;
}

bool ResourceHandle::start(Frame* frame)
{
    // If we are no longer attached to a Page, this must be an attempted load from an
    // onUnload handler, so let's just block it.
    if (!frame->page())
        return false;

    RetainPtr<CFURLRequestRef> request(AdoptCF, makeFinalRequest(d->m_request, d->m_shouldContentSniff));

    // CFURLConnection Callback API currently at version 1
    const int CFURLConnectionClientVersion = 1;
    CFURLConnectionClient client = {CFURLConnectionClientVersion, this, 0, 0, 0, willSendRequest, didReceiveResponse, didReceiveData, NULL, didFinishLoading, didFail, willCacheResponse, didReceiveChallenge};

    d->m_connection.adoptCF(CFURLConnectionCreate(0, request.get(), &client));

    CFURLConnectionScheduleWithCurrentMessageQueue(d->m_connection.get());
    CFURLConnectionScheduleDownloadWithRunLoop(d->m_connection.get(), loaderRunLoop(), kCFRunLoopDefaultMode);
    CFURLConnectionStart(d->m_connection.get());

    LOG(Network, "CFNet - Starting URL %s (handle=%p, conn=%p)", d->m_request.url().string().utf8().data(), this, d->m_connection);

    return true;
}

void ResourceHandle::cancel()
{
    if (d->m_connection) {
        CFURLConnectionCancel(d->m_connection.get());
        d->m_connection = 0;
    }
}

PassRefPtr<SharedBuffer> ResourceHandle::bufferedData()
{
    ASSERT_NOT_REACHED();
    return 0;
}

bool ResourceHandle::supportsBufferedData()
{
    return false;
}

void ResourceHandle::didReceiveAuthenticationChallenge(const AuthenticationChallenge& challenge)
{
    LOG(Network, "CFNet - didReceiveAuthenticationChallenge()");
    ASSERT(!d->m_currentCFChallenge);
    ASSERT(d->m_currentWebChallenge.isNull());
    // Since CFURLConnection networking relies on keeping a reference to the original CFURLAuthChallengeRef,
    // we make sure that is actually present
    ASSERT(challenge.cfURLAuthChallengeRef());
        
    d->m_currentCFChallenge = challenge.cfURLAuthChallengeRef();
    d->m_currentWebChallenge = AuthenticationChallenge(d->m_currentCFChallenge, this);
    
    if (client())
        client()->didReceiveAuthenticationChallenge(this, d->m_currentWebChallenge);
}

void ResourceHandle::receivedCredential(const AuthenticationChallenge& challenge, const Credential& credential)
{
    LOG(Network, "CFNet - receivedCredential()");
    ASSERT(!challenge.isNull());
    ASSERT(challenge.cfURLAuthChallengeRef());
    if (challenge != d->m_currentWebChallenge)
        return;

    CFURLCredentialRef cfCredential = createCF(credential);
    CFURLConnectionUseCredential(d->m_connection.get(), cfCredential, challenge.cfURLAuthChallengeRef());
    CFRelease(cfCredential);

    clearAuthentication();
}

void ResourceHandle::receivedRequestToContinueWithoutCredential(const AuthenticationChallenge& challenge)
{
    LOG(Network, "CFNet - receivedRequestToContinueWithoutCredential()");
    ASSERT(!challenge.isNull());
    ASSERT(challenge.cfURLAuthChallengeRef());
    if (challenge != d->m_currentWebChallenge)
        return;

    CFURLConnectionUseCredential(d->m_connection.get(), 0, challenge.cfURLAuthChallengeRef());

    clearAuthentication();
}

void ResourceHandle::receivedCancellation(const AuthenticationChallenge& challenge)
{
    LOG(Network, "CFNet - receivedCancellation()");
    if (challenge != d->m_currentWebChallenge)
        return;

    if (client())
        client()->receivedCancellation(this, challenge);
}

CFURLConnectionRef ResourceHandle::connection() const
{
    return d->m_connection.get();
}

CFURLConnectionRef ResourceHandle::releaseConnectionForDownload()
{
    LOG(Network, "CFNet - Job %p releasing connection %p for download", this, d->m_connection.get());
    return d->m_connection.releaseRef();
}

void ResourceHandle::loadResourceSynchronously(const ResourceRequest& request, ResourceError& error, ResourceResponse& response, Vector<char>& vector, Frame*)
{
    ASSERT(!request.isEmpty());
    CFURLResponseRef cfResponse = 0;
    CFErrorRef cfError = 0;
    RetainPtr<CFURLRequestRef> cfRequest(AdoptCF, makeFinalRequest(request, true));

    CFDataRef data = CFURLConnectionSendSynchronousRequest(cfRequest.get(), &cfResponse, &cfError, request.timeoutInterval());

    if (cfError) {
        error = cfError;
        CFRelease(cfError);

        response = ResourceResponse(request.url(), String(), 0, String(), String());
        response.setHTTPStatusCode(404);
    } else {
        response = cfResponse;
        if (cfResponse)
            CFRelease(cfResponse);
    }

    if (data) {
        ASSERT(vector.isEmpty());
        vector.append(CFDataGetBytePtr(data), CFDataGetLength(data));
        CFRelease(data);
    }
}

void ResourceHandle::setHostAllowsAnyHTTPSCertificate(const String& host)
{
    allowsAnyHTTPSCertificateHosts().add(host.lower());
}

void ResourceHandle::setClientCertificate(const String& host, CFDataRef cert)
{
    clientCerts().set(host.lower(), cert);
}

void ResourceHandle::setDefersLoading(bool defers)
{
    if (!d->m_connection)
        return;

    if (defers)
        CFURLConnectionHalt(d->m_connection.get());
    else
        CFURLConnectionResume(d->m_connection.get());
}

bool ResourceHandle::loadsBlocked()
{
    return false;
}

bool ResourceHandle::willLoadFromCache(ResourceRequest&)
{
    // Not having this function means that we'll ask the user about re-posting a form
    // even when we go back to a page that's still in the cache.
    notImplemented();
    return false;
}

} // namespace WebCore
