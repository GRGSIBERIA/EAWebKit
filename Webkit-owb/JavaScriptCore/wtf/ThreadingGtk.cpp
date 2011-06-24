/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Justin Haygood (jhaygood@reaktix.com)
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Threading.h"

#include <wtf/HashMap.h>
#include <wtf/MathExtras.h>

#include <glib.h>

namespace WTF {

Mutex* atomicallyInitializedStaticMutex;

static ThreadIdentifier mainThreadIdentifier;

static Mutex& threadMapMutex()
{
    static Mutex mutex;
    return mutex;
}

void initializeThreading()
{
    if (!g_thread_supported()) {
        g_thread_init(NULL);
        ASSERT(!atomicallyInitializedStaticMutex);
        atomicallyInitializedStaticMutex = new Mutex;
        threadMapMutex();
        wtf_random_init();
        mainThreadIdentifier = currentThread();
    }
    ASSERT(g_thread_supported());
}

static HashMap<ThreadIdentifier, GThread*>& threadMap()
{
    static HashMap<ThreadIdentifier, GThread*> map;
    return map;
}

static ThreadIdentifier establishIdentifierForThread(GThread*& thread)
{
    MutexLocker locker(threadMapMutex());

    static ThreadIdentifier identifierCount = 1;

    threadMap().add(identifierCount, thread);

    return identifierCount++;
}

static ThreadIdentifier identifierByGthreadHandle(GThread*& thread)
{
    MutexLocker locker(threadMapMutex());

    HashMap<ThreadIdentifier, GThread*>::iterator i = threadMap().begin();
    for (; i != threadMap().end(); ++i) {
        if (i->second == thread)
            return i->first;
    }

    return 0;
}

static GThread* threadForIdentifier(ThreadIdentifier id)
{
    MutexLocker locker(threadMapMutex());

    return threadMap().get(id);
}

static void clearThreadForIdentifier(ThreadIdentifier id)
{
    MutexLocker locker(threadMapMutex());

    ASSERT(threadMap().contains(id));

    threadMap().remove(id);
}

ThreadIdentifier createThread(ThreadFunction entryPoint, void* data)
{
    GThread* thread;
    if (!(thread = g_thread_create(entryPoint, data, TRUE, 0))) {
        LOG_ERROR("Failed to create thread at entry point %p with data %p", entryPoint, data);
        return 0;
    }

    ThreadIdentifier threadID = establishIdentifierForThread(thread);
    return threadID;
}

int waitForThreadCompletion(ThreadIdentifier threadID, void** result)
{
    ASSERT(threadID);

    GThread* thread = threadForIdentifier(threadID);

    *result = g_thread_join(thread);

    clearThreadForIdentifier(threadID);
    return 0;
}

void detachThread(ThreadIdentifier)
{
}

ThreadIdentifier currentThread()
{
    GThread* currentThread = g_thread_self();
    if (ThreadIdentifier id = identifierByGthreadHandle(currentThread))
        return id;
    return establishIdentifierForThread(currentThread);
}

bool isMainThread()
{
    return currentThread() == mainThreadIdentifier;
}

Mutex::Mutex()
    : m_mutex(g_mutex_new())
{
}

Mutex::~Mutex()
{
    g_mutex_free(m_mutex);
}

void Mutex::lock()
{
    g_mutex_lock(m_mutex);
}

bool Mutex::tryLock()
{
    return g_mutex_trylock(m_mutex);
}

void Mutex::unlock()
{
    g_mutex_unlock(m_mutex);
}

ThreadCondition::ThreadCondition()
    : m_condition(g_cond_new())
{
}

ThreadCondition::~ThreadCondition()
{
    g_cond_free(m_condition);
}

void ThreadCondition::wait(Mutex& mutex)
{
    g_cond_wait(m_condition, mutex.impl());
}

bool ThreadCondition::timedWait(Mutex& mutex, double interval)
{
    if (interval < 0.0) {
        wait(mutex);
        return true;
    }
    
    int intervalSeconds = static_cast<int>(interval);
    int intervalMicroseconds = static_cast<int>((interval - intervalSeconds) * 1000000.0);
    
    GTimeVal targetTime;
    g_get_current_time(&targetTime);
        
    targetTime.tv_sec += intervalSeconds;
    targetTime.tv_usec += intervalMicroseconds;
    if (targetTime.tv_usec > 1000000) {
        targetTime.tv_usec -= 1000000;
        targetTime.tv_sec++;
    }

    return g_cond_timed_wait(m_condition, mutex.impl(), &targetTime);
}

void ThreadCondition::signal()
{
    g_cond_signal(m_condition);
}

void ThreadCondition::broadcast()
{
    g_cond_broadcast(m_condition);
}


}
