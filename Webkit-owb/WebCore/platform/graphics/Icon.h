/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

/*
* This file was modified by Electronic Arts Inc Copyright � 2009
*/

#ifndef Icon_h
#define Icon_h

#include <wtf/RefCounted.h>
#include <wtf/Forward.h>

#if PLATFORM(MAC)
#include <wtf/RetainPtr.h>
#ifdef __OBJC__
@class NSImage;
#else
class NSImage;
#endif
#elif PLATFORM(WIN)
typedef struct HICON__* HICON;
#elif PLATFORM(QT)
#include <QIcon>
#elif PLATFORM(GTK)
#include <gdk/gdk.h>
#endif

namespace WebCore {

class GraphicsContext;
class IntRect;
class String;
    
class Icon : public RefCounted<Icon> {
public:
    static PassRefPtr<Icon> newIconForFile(const String& filename);
    ~Icon();

    void paint(GraphicsContext*, const IntRect&);

private:
#if PLATFORM(MAC)
    Icon(NSImage*);
    RetainPtr<NSImage> m_nsImage;
#elif PLATFORM(WIN)
    Icon(HICON);
    HICON m_hIcon;
#elif PLATFORM(QT)
    Icon();
    QIcon m_icon;
#elif PLATFORM(GTK)
    Icon();
    GdkPixbuf* m_icon;
#endif
};

}

#endif
