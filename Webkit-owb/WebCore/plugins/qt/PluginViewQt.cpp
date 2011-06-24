/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2008 Collabora Ltd. All rights reserved.
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
#include "PluginView.h"

#include <QWidget>
#include <QX11EmbedContainer>
#include <QX11Info>

#include "NotImplemented.h"
#include "PluginDebug.h"
#include "PluginPackage.h"
#include "npruntime_impl.h"
#include "runtime.h"
#include "runtime_root.h"
#include <kjs/JSLock.h>
#include <kjs/JSValue.h>
#include "JSDOMBinding.h"
#include "ScriptController.h"

#include "Document.h"
#include "DocumentLoader.h"
#include "Element.h"
#include "EventNames.h"
#include "FrameLoader.h"
#include "FrameLoadRequest.h"
#include "FrameTree.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "Image.h"
#include "HTMLNames.h"
#include "HTMLPlugInElement.h"
#include "KeyboardEvent.h"
#include "MouseEvent.h"
#include "Page.h"
#include "PlatformMouseEvent.h"
#include "RenderLayer.h"
#include "Settings.h"

using KJS::ExecState;
using KJS::Interpreter;
using KJS::JSLock;
using KJS::JSObject;
using KJS::JSValue;
using KJS::UString;

using std::min;

using namespace WTF;

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

void PluginView::updateWindow() const
{
    if (!parent() || !m_isWindowed)
        return;

    ASSERT(parent()->isFrameView());
    FrameView* frameView = static_cast<FrameView*>(parent());

    IntRect oldWindowRect = m_windowRect;
    IntRect oldClipRect = m_clipRect;

    m_windowRect = IntRect(frameView->contentsToWindow(frameGeometry().location()), frameGeometry().size());
    m_clipRect = windowClipRect();
    m_clipRect.move(-m_windowRect.x(), -m_windowRect.y());

    if (m_window) {
        m_window->move(m_windowRect.x(), m_windowRect.y());
        m_window->resize(m_windowRect.width(), m_windowRect.height());
        m_window->setMask(QRegion(m_clipRect.x(), m_clipRect.y(), m_clipRect.width(), m_clipRect.height()));
    }
}

void PluginView::setFocus()
{
    if (m_window)
        m_window->setFocus(Qt::OtherFocusReason);
    else
        Widget::setFocus();
}

void PluginView::show()
{
    m_isVisible = true;

    if (m_attachedToWindow && m_window)
        m_window->setVisible(true);

    Widget::show();
}

void PluginView::hide()
{
    m_isVisible = false;

    if (m_attachedToWindow && m_window)
        m_window->setVisible(false);

    Widget::hide();
}

void PluginView::paint(GraphicsContext* context, const IntRect& rect)
{
    if (!m_isStarted) {
        // Draw the "missing plugin" image
        //paintMissingPluginIcon(context, rect);
        return;
    }

    if (m_isWindowed || context->paintingDisabled())
        return;

    notImplemented();
}

void PluginView::handleKeyboardEvent(KeyboardEvent* event)
{
    notImplemented();
}

void PluginView::handleMouseEvent(MouseEvent* event)
{
    notImplemented();
}

void PluginView::setParent(ScrollView* parent)
{
    Widget::setParent(parent);

    if (parent)
        init();
    else {
        if (!m_window)
            return;
    }
}

void PluginView::setNPWindowRect(const IntRect& rect)
{
    if (!m_isStarted || !parent())
        return;

    IntPoint p = static_cast<FrameView*>(parent())->contentsToWindow(rect.location());
    m_npWindow.x = p.x();
    m_npWindow.y = p.y();

    m_npWindow.width = rect.width();
    m_npWindow.height = rect.height();

    m_npWindow.clipRect.left = 0;
    m_npWindow.clipRect.top = 0;
    m_npWindow.clipRect.right = rect.width();
    m_npWindow.clipRect.bottom = rect.height();

    if (m_npWindow.x < 0 || m_npWindow.y < 0 ||
        m_npWindow.width <= 0 || m_npWindow.height <= 0)
        return;

    if (m_plugin->pluginFuncs()->setwindow) {
        PluginView::setCurrentPluginView(this);
        KJS::JSLock::DropAllLocks dropAllLocks;
        setCallingPlugin(true);
        m_plugin->pluginFuncs()->setwindow(m_instance, &m_npWindow);
        setCallingPlugin(false);
        PluginView::setCurrentPluginView(0);

        if (!m_isWindowed)
            return;

        ASSERT(m_window);
    }
}

void PluginView::attachToWindow()
{
    if (m_attachedToWindow)
        return;

    m_attachedToWindow = true;
    if (m_isVisible && m_window)
        m_window->setVisible(true);
}

void PluginView::detachFromWindow()
{
    if (!m_attachedToWindow)
        return;

    if (m_isVisible && m_window)
        m_window->setVisible(false);
    m_attachedToWindow = false;
}

void PluginView::stop()
{
    if (!m_isStarted)
        return;

    HashSet<RefPtr<PluginStream> > streams = m_streams;
    HashSet<RefPtr<PluginStream> >::iterator end = streams.end();
    for (HashSet<RefPtr<PluginStream> >::iterator it = streams.begin(); it != end; ++it) {
        (*it)->stop();
        disconnectStream((*it).get());
    }

    ASSERT(m_streams.isEmpty());

    m_isStarted = false;

    KJS::JSLock::DropAllLocks dropAllLocks;

    // Clear the window
    m_npWindow.window = 0;
    delete (NPSetWindowCallbackStruct *)m_npWindow.ws_info;
    m_npWindow.ws_info = 0;
    if (m_plugin->pluginFuncs()->setwindow && !m_plugin->quirks().contains(PluginQuirkDontSetNullWindowHandleOnDestroy)) {
        PluginView::setCurrentPluginView(this);
        setCallingPlugin(true);
        m_plugin->pluginFuncs()->setwindow(m_instance, &m_npWindow);
        setCallingPlugin(false);
        PluginView::setCurrentPluginView(0);
    }

    // Destroy the plugin
    {
        PluginView::setCurrentPluginView(this);
        setCallingPlugin(true);
        m_plugin->pluginFuncs()->destroy(m_instance, 0);
        setCallingPlugin(false);
        PluginView::setCurrentPluginView(0);
    }

    m_instance->pdata = 0;
}

static const char* MozillaUserAgent = "Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.8.1) Gecko/20061010 Firefox/2.0";

const char* PluginView::userAgent()
{
    if (m_plugin->quirks().contains(PluginQuirkWantsMozillaUserAgent))
        return MozillaUserAgent;

    if (m_userAgent.isNull())
        m_userAgent = m_parentFrame->loader()->userAgent(m_url).utf8();

    return m_userAgent.data();
}

const char* PluginView::userAgentStatic()
{
    //FIXME - Just say we are Mozilla
    return MozillaUserAgent;
}

NPError PluginView::handlePostReadFile(Vector<char>& buffer, uint32 len, const char* buf)
{
    String filename(buf, len);

    if (filename.startsWith("file:///"))
        filename = filename.substring(8);

    if (!fileExists(filename))
        return NPERR_FILE_NOT_FOUND;

    //FIXME - read the file data into buffer
    FILE* fileHandle = fopen((filename.utf8()).data(), "r");
    
    if (fileHandle == 0)
        return NPERR_FILE_NOT_FOUND;

    //buffer.resize();

    int bytesRead = fread(buffer.data(), 1, 0, fileHandle);

    fclose(fileHandle);

    if (bytesRead <= 0)
        return NPERR_FILE_NOT_FOUND;

    return NPERR_NO_ERROR;
}

NPError PluginView::getValueStatic(NPNVariable variable, void* value)
{
    switch (variable) {
    case NPNVToolkit:
        *((uint32 *)value) = 0;
        return NPERR_NO_ERROR;

    case NPNVSupportsXEmbedBool:
        *((uint32 *)value) = true;
        return NPERR_NO_ERROR;

    case NPNVjavascriptEnabledBool:
        *((uint32 *)value) = true;
        return NPERR_NO_ERROR;

    default:
        return NPERR_GENERIC_ERROR;
    }
}

NPError PluginView::getValue(NPNVariable variable, void* value)
{
    switch (variable) {
    case NPNVxDisplay:
        if (m_window)
            *(void **)value = m_window->x11Info().display();
        else
            *(void **)value = containingWindow()->x11Info().display();
        return NPERR_NO_ERROR;                

    case NPNVxtAppContext:
        return NPERR_GENERIC_ERROR;

#if ENABLE(NETSCAPE_PLUGIN_API)
    case NPNVWindowNPObject: {
        if (m_isJavaScriptPaused)
            return NPERR_GENERIC_ERROR;

        NPObject* windowScriptObject = m_parentFrame->windowScriptNPObject();

        // Return value is expected to be retained, as described here: <http://www.mozilla.org/projects/plugin/npruntime.html>
        if (windowScriptObject)
            _NPN_RetainObject(windowScriptObject);

        void** v = (void**)value;
        *v = windowScriptObject;
            
        return NPERR_NO_ERROR;
    }

    case NPNVPluginElementNPObject: {
        if (m_isJavaScriptPaused)
            return NPERR_GENERIC_ERROR;

        NPObject* pluginScriptObject = 0;

        if (m_element->hasTagName(appletTag) || m_element->hasTagName(embedTag) || m_element->hasTagName(objectTag))
            pluginScriptObject = static_cast<HTMLPlugInElement*>(m_element)->getNPObject();

        // Return value is expected to be retained, as described here: <http://www.mozilla.org/projects/plugin/npruntime.html>
        if (pluginScriptObject)
            _NPN_RetainObject(pluginScriptObject);

        void** v = (void**)value;
        *v = pluginScriptObject;

        return NPERR_NO_ERROR;
    }
#endif

    case NPNVnetscapeWindow: {
        void* w = reinterpret_cast<void*>(value);
        *((XID *)w) = containingWindow()->winId();
        return NPERR_NO_ERROR;
    }

    default:
            return getValueStatic(variable, value);
    }
}

void PluginView::invalidateRect(NPRect* rect)
{
    notImplemented();
}

void PluginView::invalidateRegion(NPRegion region)
{
    notImplemented();
}

void PluginView::forceRedraw()
{
    notImplemented();
}

PluginView::~PluginView()
{
    stop();

    deleteAllValues(m_requests);

    freeStringArray(m_paramNames, m_paramCount);
    freeStringArray(m_paramValues, m_paramCount);

    m_parentFrame->cleanupScriptObjectsForPlugin(this);

    if (m_plugin && !(m_plugin->quirks().contains(PluginQuirkDontUnloadPlugin)))
        m_plugin->unload();

    delete m_window;
}

void PluginView::init()
{
    if (m_haveInitialized)
        return;
    m_haveInitialized = true;

    if (!m_plugin) {
        ASSERT(m_status == PluginStatusCanNotFindPlugin);
        return;
    }

    if (!m_plugin->load()) {
        m_plugin = 0;
        m_status = PluginStatusCanNotLoadPlugin;
        return;
    }

    if (!start()) {
        m_status = PluginStatusCanNotLoadPlugin;
        return;
    }

    if (m_plugin->pluginFuncs()->getvalue) {
        PluginView::setCurrentPluginView(this);
        KJS::JSLock::DropAllLocks dropAllLocks;
        setCallingPlugin(true);
        m_plugin->pluginFuncs()->getvalue(m_instance, NPPVpluginNeedsXEmbed, &m_needsXEmbed);
        setCallingPlugin(false);
        PluginView::setCurrentPluginView(0);
    }

    if (m_needsXEmbed) {
        m_window = new QX11EmbedContainer(containingWindow());
        setNativeWidget(m_window);
        setIsNPAPIPlugin(true);
    } else {
        notImplemented();
        m_status = PluginStatusCanNotLoadPlugin;
        return;
    }
    show ();

    NPSetWindowCallbackStruct *wsi = new NPSetWindowCallbackStruct();

    wsi->type = 0;

    wsi->display = m_window->x11Info().display();
    wsi->visual = (Visual*)m_window->x11Info().visual();
    wsi->depth = m_window->x11Info().depth();
    wsi->colormap = m_window->x11Info().colormap();
    m_npWindow.ws_info = wsi;

    m_npWindow.type = NPWindowTypeWindow;
    m_npWindow.window = (void*)m_window->winId();

    if (!(m_plugin->quirks().contains(PluginQuirkDeferFirstSetWindowCall)))
        setNPWindowRect(frameGeometry());

    m_status = PluginStatusLoadedSuccessfully;
}

} // namespace WebCore
