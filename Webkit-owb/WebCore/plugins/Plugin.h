/*
    Copyright (C) 2008 Trolltech ASA

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

#ifndef Plugin_h
#define Plugin_h

#include "MimeType.h"
#include <wtf/RefPtr.h>
#include <wtf/RefCounted.h>

namespace KJS {
    class ExecState;
    class JSValue;
};

namespace WebCore {

    class AtomicString;
    class Plugin;
    class String;

    // FIXME: Generated JSPlugin.cpp doesn't include JSMimeType.h for toJS
    KJS::JSValue* toJS(KJS::ExecState*, MimeType*);

    class PluginData;

    class Plugin : public RefCounted<Plugin> {
    public:
        static PassRefPtr<Plugin> create(PluginData* pluginData, unsigned index) { return adoptRef(new Plugin(pluginData, index)); }
        ~Plugin();

        String name() const;
        String filename() const;
        String description() const;

        unsigned length() const;

        PassRefPtr<MimeType> item(unsigned index);
        bool canGetItemsForName(const AtomicString& propertyName);
        PassRefPtr<MimeType> nameGetter(const AtomicString& propertyName);

    private:
        Plugin(PluginData*, unsigned index);
        RefPtr<PluginData> m_pluginData;
        unsigned m_index;
    };

}

#endif
