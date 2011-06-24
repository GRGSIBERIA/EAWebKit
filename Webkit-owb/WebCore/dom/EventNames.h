/*
 * Copyright (C) 2005, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Jon Shier (jshier@iastate.edu)
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

#ifndef EventNames_h
#define EventNames_h

#include <wtf/FastAllocBase.h>
#include "AtomicString.h"

namespace WebCore {

#define DOM_EVENT_NAMES_FOR_EACH(macro) \
    \
    macro(abort) \
    macro(beforecopy) \
    macro(beforecut) \
    macro(beforepaste) \
    macro(beforeunload) \
    macro(blur) \
    macro(cached) \
    macro(change) \
    macro(checking) \
    macro(click) \
    macro(contextmenu) \
    macro(copy) \
    macro(cut) \
    macro(dblclick) \
    macro(downloading) \
    macro(drag) \
    macro(dragend) \
    macro(dragenter) \
    macro(dragleave) \
    macro(dragover) \
    macro(dragstart) \
    macro(drop) \
    macro(error) \
    macro(focus) \
    macro(input) \
    macro(keydown) \
    macro(keypress) \
    macro(keyup) \
    macro(load) \
    macro(loadstart) \
    macro(message) \
    macro(mousedown) \
    macro(mousemove) \
    macro(mouseout) \
    macro(mouseover) \
    macro(mouseup) \
    macro(mousewheel) \
    macro(noupdate) \
    macro(offline) \
    macro(online) \
    macro(overflowchanged) \
    macro(paste) \
    macro(readystatechange) \
    macro(reset) \
    macro(resize) \
    macro(scroll) \
    macro(search) \
    macro(select) \
    macro(selectstart) \
    macro(storage) \
    macro(submit) \
    macro(textInput) \
    macro(unload) \
    macro(updateready) \
    macro(zoom) \
    \
    macro(DOMActivate) \
    macro(DOMAttrModified) \
    macro(DOMCharacterDataModified) \
    macro(DOMFocusIn) \
    macro(DOMFocusOut) \
    macro(DOMNodeInserted) \
    macro(DOMNodeInsertedIntoDocument) \
    macro(DOMNodeRemoved) \
    macro(DOMNodeRemovedFromDocument) \
    macro(DOMSubtreeModified) \
    macro(DOMContentLoaded) \
    \
    macro(webkitBeforeTextInserted) \
    macro(webkitEditableContentChanged) \
    \
    macro(canshowcurrentframe) \
    macro(canplay) \
    macro(canplaythrough) \
    macro(dataunavailable) \
    macro(durationchange) \
    macro(emptied) \
    macro(ended) \
    macro(loadedfirstframe) \
    macro(loadedmetadata) \
    macro(pause) \
    macro(play) \
    macro(ratechange) \
    macro(timeupdate) \
    macro(volumechange) \
    macro(waiting) \
    \
    macro(begin) \
    macro(progress) \
    macro(stalled) \
    \
// end of DOM_EVENT_NAMES_FOR_EACH

    class EventNames: public WTF::FastAllocBase {
        EventNames();
        int dummy; // Needed to make initialization macro work.

    public:
        ~EventNames();
        static void init();
        static void unInit();

        #define DOM_EVENT_NAMES_DECLARE(name) AtomicString name##Event;
        DOM_EVENT_NAMES_FOR_EACH(DOM_EVENT_NAMES_DECLARE)
        #undef DOM_EVENT_NAMES_DECLARE
    };

    EventNames& eventNames();

}

#endif
