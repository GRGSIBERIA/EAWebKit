/*
    Copyright (C) 2007 Trolltech ASA

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

/*
* This file was modified by Electronic Arts Inc Copyright � 2009
*/

#ifndef Navigator_h
#define Navigator_h

#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

    class Frame;
    class MimeTypeArray;
    class PluginData;
    class PluginArray;
    class String;

    class Navigator : public RefCounted<Navigator> {
    public:
        static PassRefPtr<Navigator> create(Frame* frame) { return adoptRef(new Navigator(frame)); }
        ~Navigator();

        void disconnectFrame();
        Frame* frame() const { return m_frame; }

        String appCodeName() const;
        String appName() const;
        String appVersion() const;
        String language() const;
        String userAgent() const;
        String platform() const;
        PluginArray* plugins() const;
        MimeTypeArray* mimeTypes() const;
        String product() const;
        String productSub() const;
        String vendor() const;
        String vendorSub() const;
        bool cookieEnabled() const;
        bool javaEnabled() const;

        bool onLine() const;
    private:
        Navigator(Frame*);
        Frame* m_frame;
        mutable RefPtr<PluginArray> m_plugins;
        mutable RefPtr<MimeTypeArray> m_mimeTypes;
    };

}

#endif
