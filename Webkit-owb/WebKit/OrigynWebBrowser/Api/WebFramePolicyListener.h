/*
 * Copyright (C) 2008 Pleyo.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Pleyo nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PLEYO AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL PLEYO OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
* This file was modified by Electronic Arts Inc Copyright � 2009
*/

#ifndef WebFramePolicyListener_h
#define WebFramePolicyListener_h

#include <wtf/FastAllocBase.h>

/**
 *  @file  WebFramePolicyListener.h
 *  WebFramePolicyListener description
 *  Repository informations :
 * - $URL$
 * - $Rev$
 * - $Date$
 */
#include "BALBase.h"
#include <wtf/PassRefPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

#include <FrameLoaderTypes.h>

namespace WebCore {
    class Frame;
}

class WebFramePolicyListener: public WTF::FastAllocBase {
public:

    /**
     *  createInstance description
     * @param[in]: description
     * @param[out]: description
     * @code
     * @endcode
     */
    static WebFramePolicyListener* createInstance(PassRefPtr<WebCore::Frame>);

    /**
     *  ~WebFramePolicyListener description
     * @param[in]: description
     * @param[out]: description
     * @code
     * @endcode
     */
    virtual ~WebFramePolicyListener();
protected:

    /**
     *  WebFramePolicyListener description
     * @param[in]: description
     * @param[out]: description
     * @code
     * @endcode
     */
    WebFramePolicyListener(PassRefPtr<WebCore::Frame>);

public:

    /**
     *  use description
     * @param[in]: description
     * @param[out]: description
     * @code
     * @endcode
     */
    virtual void use(void);

    /**
     *  download description
     * @param[in]: description
     * @param[out]: description
     * @code
     * @endcode
     */
    virtual void download(void);

    /**
     *  ignore description
     * @param[in]: description
     * @param[out]: description
     * @code
     * @endcode
     */
    virtual void ignore(void);


    /**
     *  continueSubmit description
     * @param[in]: description
     * @param[out]: description
     * @code
     * @endcode
     */
    virtual void continueSubmit(void);


    /**
     *  receivedPolicyDecision description
     * @param[in]: description
     * @param[out]: description
     * @code
     * @endcode
     */
    void receivedPolicyDecision(WebCore::PolicyAction);

    /**
     *  invalidate description
     * @param[in]: description
     * @param[out]: description
     * @code
     * @endcode
     */
    void invalidate();
private:
    RefPtr<WebCore::Frame> m_frame;
};

#endif
