/*
 *  Copyright (C) 2008 Trolltech ASA
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "JSPlugin.h"

#include "AtomicString.h"
#include "Plugin.h"

namespace WebCore {

using namespace KJS;

bool JSPlugin::canGetItemsForName(ExecState*, Plugin* plugin, const Identifier& propertyName)
{
    return plugin->canGetItemsForName(propertyName);
}

JSValue* JSPlugin::nameGetter(ExecState* exec, const Identifier& propertyName, const PropertySlot& slot)
{
    JSPlugin* thisObj = static_cast<JSPlugin*>(slot.slotBase());
    return toJS(exec, thisObj->impl()->nameGetter(propertyName));
}

}
