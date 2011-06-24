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

#include "config.h"
#include "WebContextMenuClient.h"

#include "WebDownload.h"
#include "WebElementPropertyBag.h"
#include "WebView.h"

#include <ContextMenu.h>
#include <FrameLoader.h>
#include <FrameLoadRequest.h>
#include <Page.h>
#include <ResourceRequest.h>
#include <NotImplemented.h>
#include "BAL/Includes/FakedDeepsee.h"

using namespace WebCore;

WebContextMenuClient::WebContextMenuClient(WebView* webView)
    : m_webView(webView)
{
    DS_CONSTRUCT();
}

WebContextMenuClient::~WebContextMenuClient()
{
    DS_DESTRUCT();
}

void WebContextMenuClient::contextMenuDestroyed()
{
    delete this;
}

PlatformMenuDescription WebContextMenuClient::getCustomMenuFromDefaultItems(ContextMenu* menu)
{
    return menu->releasePlatformDescription();
}

/*HMENU WebContextMenuClient::getCustomMenuFromDefaultItems(ContextMenu* menu)
{
    COMPtr<IWebUIDelegate> uiDelegate;
    if (FAILED(m_webView->uiDelegate(&uiDelegate)))
        return menu->platformDescription();

    ASSERT(uiDelegate);

    HMENU newMenu = 0;
    COMPtr<WebElementPropertyBag> propertyBag;
    propertyBag.adoptRef(WebElementPropertyBag::createInstance(menu->hitTestResult()));
    // FIXME: We need to decide whether to do the default before calling this delegate method
    if (FAILED(uiDelegate->contextMenuItemsForElement(m_webView, propertyBag.get(), (OLE_HANDLE)(ULONG64)menu->platformDescription(), (OLE_HANDLE*)&newMenu)))
        return menu->platformDescription();
    return fixMenuReceivedFromOldSafari(uiDelegate.get(), menu, newMenu);
}*/

void WebContextMenuClient::contextMenuItemSelected(ContextMenuItem* item, const ContextMenu* parentMenu)
{
    /*ASSERT(item->type() == ActionType || item->type() == CheckableActionType);

    COMPtr<IWebUIDelegate> uiDelegate;
    if (FAILED(m_webView->uiDelegate(&uiDelegate)))
        return;

    ASSERT(uiDelegate);

    COMPtr<WebElementPropertyBag> propertyBag;
    propertyBag.adoptRef(WebElementPropertyBag::createInstance(parentMenu->hitTestResult()));
            
    uiDelegate->contextMenuItemSelected(m_webView, item->releasePlatformDescription(), propertyBag.get());*/
}

void WebContextMenuClient::downloadURL(const KURL& url)
{
    DefaultDownloadDelegate* downloadDelegate = m_webView->downloadDelegate();

    // Its the delegate's job to ref the WebDownload to keep it alive - otherwise it will be destroyed
    // when this method returns
    WebDownload* download = WebDownload::createInstance(url, downloadDelegate);
    download->start();
    delete download;
}

void WebContextMenuClient::copyImageToClipboard(const HitTestResult&)
{
    notImplemented();
}


void WebContextMenuClient::searchWithGoogle(const Frame* frame)
{
    String searchString = frame->selectedText();
    searchString.stripWhiteSpace();
    String encoded = encodeWithURLEscapeSequences(searchString);
    encoded.replace("%20", "+");
    
    String url("http://www.google.com/search?q=");
    url.append(encoded);
    url.append("&ie=UTF-8&oe=UTF-8");

    ResourceRequest request = ResourceRequest(url);
    if (Page* page = frame->page())
        page->mainFrame()->loader()->urlSelected(FrameLoadRequest(request), 0, false, true);
}

void WebContextMenuClient::lookUpInDictionary(Frame*)
{
    notImplemented();
}

void WebContextMenuClient::speak(const String&)
{
    notImplemented();
}

void WebContextMenuClient::stopSpeaking()
{
    notImplemented();
}
