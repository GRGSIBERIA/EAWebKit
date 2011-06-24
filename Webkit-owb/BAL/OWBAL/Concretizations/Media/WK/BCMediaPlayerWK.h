/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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

/*
* This file was modified by Electronic Arts Inc Copyright � 2009
*/

#ifndef MediaPlayer_h
#define MediaPlayer_h

#include <wtf/FastAllocBase.h>
#if ENABLE(VIDEO)

#include "IntRect.h"
#include "StringHash.h"
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>

namespace OWBAL {

class FrameView;
class GraphicsContext;
class IntSize;
class MediaPlayer;
class MediaPlayerPrivate;
class String;

class MediaPlayerClient: public WTF::FastAllocBase {
public:
    virtual ~MediaPlayerClient() { }
    virtual void mediaPlayerNetworkStateChanged(MediaPlayer*) { }
    virtual void mediaPlayerReadyStateChanged(MediaPlayer*) { }
    virtual void mediaPlayerVolumeChanged(MediaPlayer*) { }
    virtual void mediaPlayerTimeChanged(MediaPlayer*) { }
    virtual void mediaPlayerRepaint(MediaPlayer*) { }
};

class MediaPlayer : Noncopyable {
public:
        // Placement operator new.
        void* operator new(size_t, void* p) { return p; }
        void* operator new[](size_t, void* p) { return p; }

        void* operator new(size_t size)
        {
            void* p = fastMalloc(size);
            fastMallocMatchValidateMalloc(p, WTF::Internal::AllocTypeClassNew);
            return p;
        }

        void operator delete(void* p)
        {
            fastMallocMatchValidateFree(p, WTF::Internal::AllocTypeClassNew);
            fastFree(p);  // We don't need to check for a null pointer; the compiler does this.
        }

        void* operator new[](size_t size)
        {
            void* p = fastMalloc(size);
            fastMallocMatchValidateMalloc(p, WTF::Internal::AllocTypeClassNewArray);
            return p;
        }

        void operator delete[](void* p)
        {
            fastMallocMatchValidateFree(p, WTF::Internal::AllocTypeClassNewArray);
            fastFree(p);  // We don't need to check for a null pointer; the compiler does this.
        }
public:
    MediaPlayer(MediaPlayerClient*);
    virtual ~MediaPlayer();
    
    static bool isAvailable();
    static bool supportsType(const String&);
    static void getSupportedTypes(HashSet<String>&);
    
    IntSize naturalSize();
    bool hasVideo();
    
    void setFrameView(FrameView* frameView) { m_frameView = frameView; }
    
    IntRect rect() const { return m_rect; }
    void setRect(const IntRect& r);
    
    void load(const String& url);
    void cancelLoad();
    
    bool visible() const;
    void setVisible(bool);
    
    void play();
    void pause();    
    
    bool paused() const;
    bool seeking() const;
    
    float duration() const;
    float currentTime() const;
    void seek(float time);
    
    void setEndTime(float time);
    
    float rate() const;
    void setRate(float);
    
    float maxTimeBuffered();
    float maxTimeSeekable();
    
    unsigned bytesLoaded();
    bool totalBytesKnown();
    unsigned totalBytes();
    
    float volume() const;
    void setVolume(float);
    
    int dataRate() const;
    
    void paint(GraphicsContext*, const IntRect&);
    
    enum NetworkState { Empty, LoadFailed, Loading, LoadedMetaData, LoadedFirstFrame, Loaded };
    NetworkState networkState();

    enum ReadyState  { DataUnavailable, CanShowCurrentFrame, CanPlay, CanPlayThrough };
    ReadyState readyState();
    
    void networkStateChanged();
    void readyStateChanged();
    void volumeChanged();
    void timeChanged();

    void repaint();
    
private:
        
    friend class MediaPlayerPrivate;
    
    MediaPlayerClient* m_mediaPlayerClient;
    MediaPlayerPrivate* m_private;
    FrameView* m_frameView;
    IntRect m_rect;
    bool m_visible;
    float m_rate;
    float m_volume;
};

}

#endif
#endif
