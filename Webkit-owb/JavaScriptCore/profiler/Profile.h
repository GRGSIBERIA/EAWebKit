/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
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

#ifndef Profile_h
#define Profile_h

#include "ProfileNode.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace KJS {

    class ExecState;
    class ProfilerClient;

    class Profile : public RefCounted<Profile> {
    public:
        static PassRefPtr<Profile> create(const UString& title, ExecState* originatingGlobalExec, unsigned pageGroupIdentifier, ProfilerClient*);

        void willExecute(const CallIdentifier&);
        void didExecute(const CallIdentifier&);
        void stopProfiling();
        bool didFinishAllExecution();

        const UString& title() const { return m_title; };
        ProfileNode* callTree() const { return m_head.get(); };

        double totalTime() const { return m_head->totalTime(); }

        ExecState* originatingGlobalExec() const { return m_originatingGlobalExec; }
        unsigned pageGroupIdentifier() const { return m_pageGroupIdentifier; }
        ProfilerClient* client() { return m_client; }

        void forEach(void (ProfileNode::*)());
        void sortTotalTimeDescending() { forEach(&ProfileNode::sortTotalTimeDescending); }
        void sortTotalTimeAscending() { forEach(&ProfileNode::sortTotalTimeAscending); }
        void sortSelfTimeDescending() { forEach(&ProfileNode::sortSelfTimeDescending); }
        void sortSelfTimeAscending() { forEach(&ProfileNode::sortSelfTimeAscending); }
        void sortCallsDescending() { forEach(&ProfileNode::sortCallsDescending); }
        void sortCallsAscending() { forEach(&ProfileNode::sortCallsAscending); }
        void sortFunctionNameDescending() { forEach(&ProfileNode::sortFunctionNameDescending); }
        void sortFunctionNameAscending() { forEach(&ProfileNode::sortFunctionNameAscending); }

        void focus(const ProfileNode* profileNode);
        void exclude(const ProfileNode* profileNode);
        void restoreAll();

        bool stoppedProfiling() { return m_stoppedProfiling; }

#ifndef NDEBUG
        void debugPrintData() const;
        void debugPrintDataSampleStyle() const;
#endif
        typedef void (Profile::*ProfileFunction)(const CallIdentifier& callIdentifier);

    private:
        Profile(const UString& title, ExecState* originatingGlobalExec, unsigned pageGroupIdentifier, ProfilerClient*);

        void removeProfileStart();
        void removeProfileEnd();

        UString m_title;
        ExecState* m_originatingGlobalExec;
        unsigned m_pageGroupIdentifier;
        unsigned m_stoppedCallDepth;
        RefPtr<ProfileNode> m_head;
        RefPtr<ProfileNode> m_currentNode;
        ProfilerClient* m_client;
        bool m_stoppedProfiling;
    };

} // namespace KJS

#endif // Profiler_h
