/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

/*
* This file was modified by Electronic Arts Inc Copyright � 2009
*/

#include "config.h"
#include "InspectorController.h"

#include "CString.h"
#include "CachedResource.h"
#include "Console.h"
#include "DOMWindow.h"
#include "DocLoader.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "Element.h"
#include "FloatConversion.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLFrameOwnerElement.h"
#include "InspectorClient.h"
#include "JavaScriptCallFrame.h"
#include "JSDOMWindow.h"
#include "JSInspectedObjectWrapper.h"
#include "JSInspectorCallbackWrapper.h"
#include "JSJavaScriptCallFrame.h"
#include "JSNode.h"
#include "JSRange.h"
#include "JavaScriptDebugServer.h"
#include "JavaScriptProfile.h"
#include "Page.h"
#include "Range.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "Settings.h"
#include "SharedBuffer.h"
#include "SystemTime.h"
#include "TextEncoding.h"
#include "TextIterator.h"
#include "ScriptController.h"
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/JSRetainPtr.h>
#include <JavaScriptCore/JSStringRef.h>
#include <kjs/ustring.h>
#include <profiler/Profile.h>
#include <profiler/Profiler.h>
#include <wtf/RefCounted.h>

#if ENABLE(DATABASE)
#include "Database.h"
#include "JSDatabase.h"
#endif

using namespace KJS;
using namespace std;

namespace WebCore {

static const char* const UserInitiatedProfileName = "org.webkit.profiles.user-initiated";

static JSRetainPtr<JSStringRef> jsStringRef(const char* str)
{
    return JSRetainPtr<JSStringRef>(Adopt, JSStringCreateWithUTF8CString(str));
}

static JSRetainPtr<JSStringRef> jsStringRef(const SourceProvider& str)
{
    return JSRetainPtr<JSStringRef>(Adopt, JSStringCreateWithCharacters(str.data(), str.length()));
}

static JSRetainPtr<JSStringRef> jsStringRef(const String& str)
{
    return JSRetainPtr<JSStringRef>(Adopt, JSStringCreateWithCharacters(str.characters(), str.length()));
}

static JSRetainPtr<JSStringRef> jsStringRef(const UString& str)
{
    JSLock lock;
    return JSRetainPtr<JSStringRef>(toRef(str.rep()));
}

static String toString(JSContextRef context, JSValueRef value, JSValueRef* exception)
{
    ASSERT_ARG(value, value);
    if (!value)
        return String();
    JSRetainPtr<JSStringRef> scriptString(Adopt, JSValueToStringCopy(context, value, exception));
    if (exception && *exception)
        return String();
    return String(JSStringGetCharactersPtr(scriptString.get()), JSStringGetLength(scriptString.get()));
}

#define HANDLE_EXCEPTION(context, exception) handleException((context), (exception), __LINE__)

JSValueRef InspectorController::callSimpleFunction(JSContextRef context, JSObjectRef thisObject, const char* functionName) const
{
    JSValueRef exception = 0;
    return callFunction(context, thisObject, functionName, 0, 0, exception);
}

JSValueRef InspectorController::callFunction(JSContextRef context, JSObjectRef thisObject, const char* functionName, size_t argumentCount, const JSValueRef arguments[], JSValueRef& exception) const
{
    ASSERT_ARG(context, context);
    ASSERT_ARG(thisObject, thisObject);

    if (exception)
        return JSValueMakeUndefined(context);

    JSValueRef functionProperty = JSObjectGetProperty(context, thisObject, jsStringRef(functionName).get(), &exception);
    if (HANDLE_EXCEPTION(context, exception))
        return JSValueMakeUndefined(context);

    JSObjectRef function = JSValueToObject(context, functionProperty, &exception);
    if (HANDLE_EXCEPTION(context, exception))
        return JSValueMakeUndefined(context);

    JSValueRef result = JSObjectCallAsFunction(context, function, thisObject, argumentCount, arguments, &exception);
    if (HANDLE_EXCEPTION(context, exception))
        return JSValueMakeUndefined(context);

    return result;
}

// ConsoleMessage Struct

struct ConsoleMessage: public WTF::FastAllocBase {
    ConsoleMessage(MessageSource s, MessageLevel l, const String& m, unsigned li, const String& u)
        : source(s)
        , level(l)
        , message(m)
        , line(li)
        , url(u)
    {
    }

    ConsoleMessage(MessageSource s, MessageLevel l, ExecState* exec, const ArgList& args, unsigned li, const String& u)
        : source(s)
        , level(l)
        , wrappedArguments(args.size())
        , line(li)
        , url(u)
    {
        JSLock lock;
        for (unsigned i = 0; i < args.size(); ++i)
            wrappedArguments[i] = JSInspectedObjectWrapper::wrap(exec, args[i]);
    }

    MessageSource source;
    MessageLevel level;
    String message;
    Vector<ProtectedPtr<JSValue> > wrappedArguments;
    unsigned line;
    String url;
};

// XMLHttpRequestResource Class

struct XMLHttpRequestResource: public WTF::FastAllocBase {
    XMLHttpRequestResource(KJS::UString& sourceString)
    {
        KJS::JSLock lock;
        this->sourceString = sourceString.rep();
    }

    ~XMLHttpRequestResource()
    {
        KJS::JSLock lock;
        sourceString.clear();
    }

    RefPtr<KJS::UString::Rep> sourceString;
};

// InspectorResource Struct

struct InspectorResource : public RefCounted<InspectorResource> {
    // Keep these in sync with WebInspector.Resource.Type
    enum Type {
        Doc,
        Stylesheet,
        Image,
        Font,
        Script,
        XHR,
        Media,
        Other
    };

    static PassRefPtr<InspectorResource> create(long long identifier, DocumentLoader* documentLoader, Frame* frame)
    {
        return adoptRef(new InspectorResource(identifier, documentLoader, frame));
    }
    
    ~InspectorResource()
    {
        setScriptObject(0, 0);
    }

    Type type() const
    {
        if (xmlHttpRequestResource)
            return XHR;

        if (requestURL == loader->requestURL())
            return Doc;

        if (loader->frameLoader() && requestURL == loader->frameLoader()->iconURL())
            return Image;

        CachedResource* cachedResource = frame->document()->docLoader()->cachedResource(requestURL.string());
        if (!cachedResource)
            return Other;

        switch (cachedResource->type()) {
            case CachedResource::ImageResource:
                return Image;
            case CachedResource::FontResource:
                return Font;
            case CachedResource::CSSStyleSheet:
#if ENABLE(XSLT)
            case CachedResource::XSLStyleSheet:
#endif
                return Stylesheet;
            case CachedResource::Script:
                return Script;
            default:
                return Other;
        }
    }

    void setScriptObject(JSContextRef context, JSObjectRef newScriptObject)
    {
        if (scriptContext && scriptObject)
            JSValueUnprotect(scriptContext, scriptObject);

        scriptObject = newScriptObject;
        scriptContext = context;

        ASSERT((context && newScriptObject) || (!context && !newScriptObject));
        if (context && newScriptObject)
            JSValueProtect(context, newScriptObject);
    }

    void setXMLHttpRequestProperties(KJS::UString& data)
    {
        xmlHttpRequestResource.set(new XMLHttpRequestResource(data));
    }
    
    String sourceString() const
     {
         if (xmlHttpRequestResource)
            return KJS::UString(xmlHttpRequestResource->sourceString);

        RefPtr<SharedBuffer> buffer;
        String textEncodingName;

        if (requestURL == loader->requestURL()) {
            buffer = loader->mainResourceData();
            textEncodingName = frame->document()->inputEncoding();
        } else {
            CachedResource* cachedResource = frame->document()->docLoader()->cachedResource(requestURL.string());
            if (!cachedResource)
                return String();

            buffer = cachedResource->data();
            textEncodingName = cachedResource->encoding();
        }

        if (!buffer)
            return String();

        TextEncoding encoding(textEncodingName);
        if (!encoding.isValid())
            encoding = WindowsLatin1Encoding();
        return encoding.decode(buffer->data(), buffer->size());
     }

    long long identifier;
    RefPtr<DocumentLoader> loader;
    RefPtr<Frame> frame;
    OwnPtr<XMLHttpRequestResource> xmlHttpRequestResource;
    KURL requestURL;
    HTTPHeaderMap requestHeaderFields;
    HTTPHeaderMap responseHeaderFields;
    String mimeType;
    String suggestedFilename;
    JSContextRef scriptContext;
    JSObjectRef scriptObject;
    long long expectedContentLength;
    bool cached;
    bool finished;
    bool failed;
    int length;
    int responseStatusCode;
    double startTime;
    double responseReceivedTime;
    double endTime;

protected:
    InspectorResource(long long identifier, DocumentLoader* documentLoader, Frame* frame)
        : identifier(identifier)
        , loader(documentLoader)
        , frame(frame)
        , xmlHttpRequestResource(0)
        , scriptContext(0)
        , scriptObject(0)
        , expectedContentLength(0)
        , cached(false)
        , finished(false)
        , failed(false)
        , length(0)
        , responseStatusCode(0)
        , startTime(-1.0)
        , responseReceivedTime(-1.0)
        , endTime(-1.0)
    {
    }
};

// InspectorDatabaseResource Struct

#if ENABLE(DATABASE)
struct InspectorDatabaseResource : public RefCounted<InspectorDatabaseResource> {
    static PassRefPtr<InspectorDatabaseResource> create(Database* database, const String& domain, const String& name, const String& version)
    {
        return adoptRef(new InspectorDatabaseResource(database, domain, name, version));
    }

    void setScriptObject(JSContextRef context, JSObjectRef newScriptObject)
    {
        if (scriptContext && scriptObject)
            JSValueUnprotect(scriptContext, scriptObject);

        scriptObject = newScriptObject;
        scriptContext = context;

        ASSERT((context && newScriptObject) || (!context && !newScriptObject));
        if (context && newScriptObject)
            JSValueProtect(context, newScriptObject);
    }

    RefPtr<Database> database;
    String domain;
    String name;
    String version;
    JSContextRef scriptContext;
    JSObjectRef scriptObject;
    
private:
    InspectorDatabaseResource(Database* database, const String& domain, const String& name, const String& version)
        : database(database)
        , domain(domain)
        , name(name)
        , version(version)
        , scriptContext(0)
        , scriptObject(0)
    {
    }
};
#endif

// JavaScript Callbacks

static bool addSourceToFrame(const String& mimeType, const String& source, Node* frameNode)
{
    ASSERT_ARG(frameNode, frameNode);

    if (!frameNode)
        return false;

    if (!frameNode->attached()) {
        ASSERT_NOT_REACHED();
        return false;
    }

    ASSERT(frameNode->isElementNode());
    if (!frameNode->isElementNode())
        return false;

    Element* element = static_cast<Element*>(frameNode);
    ASSERT(element->isFrameOwnerElement());
    if (!element->isFrameOwnerElement())
        return false;

    HTMLFrameOwnerElement* frameOwner = static_cast<HTMLFrameOwnerElement*>(element);
    ASSERT(frameOwner->contentFrame());
    if (!frameOwner->contentFrame())
        return false;

    FrameLoader* loader = frameOwner->contentFrame()->loader();

    loader->setResponseMIMEType(mimeType);
    loader->begin();
    loader->write(source);
    loader->end();

    return true;
}

static JSValueRef addResourceSourceToFrame(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSValueRef undefined = JSValueMakeUndefined(ctx);

    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (argumentCount < 2 || !controller)
        return undefined;

    JSValueRef identifierValue = arguments[0];
    if (!JSValueIsNumber(ctx, identifierValue))
        return undefined;

    long long identifier = static_cast<long long>(JSValueToNumber(ctx, identifierValue, exception));
    if (exception && *exception)
        return undefined;

    RefPtr<InspectorResource> resource = controller->resources().get(identifier);
    ASSERT(resource);
    if (!resource)
        return undefined;

    String sourceString = resource->sourceString();
    if (sourceString.isEmpty())
        return undefined;

    addSourceToFrame(resource->mimeType, sourceString, toNode(toJS(arguments[1])));

    return undefined;
}

static JSValueRef addSourceToFrame(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSValueRef undefined = JSValueMakeUndefined(ctx);

    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (argumentCount < 3 || !controller)
        return undefined;

    JSValueRef mimeTypeValue = arguments[0];
    if (!JSValueIsString(ctx, mimeTypeValue))
        return undefined;

    JSValueRef sourceValue = arguments[1];
    if (!JSValueIsString(ctx, sourceValue))
        return undefined;

    String mimeType = toString(ctx, mimeTypeValue, exception);
    if (mimeType.isEmpty())
        return undefined;

    String source = toString(ctx, sourceValue, exception);
    if (source.isEmpty())
        return undefined;

    addSourceToFrame(mimeType, source, toNode(toJS(arguments[2])));

    return undefined;
}

static JSValueRef getResourceDocumentNode(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSValueRef undefined = JSValueMakeUndefined(ctx);

    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!argumentCount || argumentCount > 1 || !controller)
        return undefined;

    JSValueRef identifierValue = arguments[0];
    if (!JSValueIsNumber(ctx, identifierValue))
        return undefined;

    long long identifier = static_cast<long long>(JSValueToNumber(ctx, identifierValue, exception));
    if (exception && *exception)
        return undefined;

    RefPtr<InspectorResource> resource = controller->resources().get(identifier);
    ASSERT(resource);
    if (!resource)
        return undefined;

    Frame* frame = resource->frame.get();

    Document* document = frame->document();
    if (!document)
        return undefined;

    if (document->isPluginDocument() || document->isImageDocument() || document->isMediaDocument())
        return undefined;

    ExecState* exec = toJSDOMWindowShell(resource->frame.get())->window()->globalExec();

    KJS::JSLock lock;
    JSValueRef documentValue = toRef(JSInspectedObjectWrapper::wrap(exec, toJS(exec, document)));
    return documentValue;
}

static JSValueRef highlightDOMNode(JSContextRef context, JSObjectRef /*function*/, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* /*exception*/)
{
    JSValueRef undefined = JSValueMakeUndefined(context);

    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (argumentCount < 1 || !controller)
        return undefined;

    JSQuarantinedObjectWrapper* wrapper = JSQuarantinedObjectWrapper::asWrapper(toJS(arguments[0]));
    if (!wrapper)
        return undefined;
    Node* node = toNode(wrapper->unwrappedObject());
    if (!node)
        return undefined;

    controller->highlight(node);

    return undefined;
}

static JSValueRef hideDOMNodeHighlight(JSContextRef context, JSObjectRef /*function*/, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* /*exception*/)
{
    JSValueRef undefined = JSValueMakeUndefined(context);

    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (argumentCount || !controller)
        return undefined;

    controller->hideHighlight();

    return undefined;
}

static JSValueRef loaded(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef[] /*arguments[]*/, JSValueRef* /*exception*/)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    controller->scriptObjectReady();
    return JSValueMakeUndefined(ctx);
}

static JSValueRef unloading(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef[] /*arguments[]*/, JSValueRef* /*exception*/)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    controller->close();
    return JSValueMakeUndefined(ctx);
}

static JSValueRef attach(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef[] /*arguments[]*/, JSValueRef* /*exception*/)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    controller->attachWindow();
    return JSValueMakeUndefined(ctx);
}

static JSValueRef detach(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef[] /*arguments[]*/, JSValueRef* /*exception*/)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    controller->detachWindow();
    return JSValueMakeUndefined(ctx);
}

static JSValueRef search(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    if (argumentCount < 2 || !JSValueIsString(ctx, arguments[1]))
        return JSValueMakeUndefined(ctx);

    Node* node = toNode(toJS(arguments[0]));
    if (!node)
        return JSValueMakeUndefined(ctx);

    String target = toString(ctx, arguments[1], exception);

    JSObjectRef global = JSContextGetGlobalObject(ctx);

    JSValueRef arrayProperty = JSObjectGetProperty(ctx, global, jsStringRef("Array").get(), exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    JSObjectRef arrayConstructor = JSValueToObject(ctx, arrayProperty, exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    JSObjectRef result = JSObjectCallAsConstructor(ctx, arrayConstructor, 0, 0, exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    JSValueRef pushProperty = JSObjectGetProperty(ctx, result, jsStringRef("push").get(), exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    JSObjectRef pushFunction = JSValueToObject(ctx, pushProperty, exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    RefPtr<Range> searchRange(rangeOfContents(node));

    ExceptionCode ec = 0;
    do {
        RefPtr<Range> resultRange(findPlainText(searchRange.get(), target, true, false));
        if (resultRange->collapsed(ec))
            break;

        // A non-collapsed result range can in some funky whitespace cases still not
        // advance the range's start position (4509328). Break to avoid infinite loop.
        VisiblePosition newStart = endVisiblePosition(resultRange.get(), DOWNSTREAM);
        if (newStart == startVisiblePosition(searchRange.get(), DOWNSTREAM))
            break;

        KJS::JSLock lock;
        JSValueRef arg0 = toRef(toJS(toJS(ctx), resultRange.get()));
        JSObjectCallAsFunction(ctx, pushFunction, result, 1, &arg0, exception);
        if (exception && *exception)
            return JSValueMakeUndefined(ctx);

        setStart(searchRange.get(), newStart);
    } while (true);

    return result;
}

#if ENABLE(DATABASE)
static JSValueRef databaseTableNames(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    if (argumentCount < 1)
        return JSValueMakeUndefined(ctx);

    JSQuarantinedObjectWrapper* wrapper = JSQuarantinedObjectWrapper::asWrapper(toJS(arguments[0]));
    if (!wrapper)
        return JSValueMakeUndefined(ctx);

    Database* database = toDatabase(wrapper->unwrappedObject());
    if (!database)
        return JSValueMakeUndefined(ctx);

    JSObjectRef global = JSContextGetGlobalObject(ctx);

    JSValueRef arrayProperty = JSObjectGetProperty(ctx, global, jsStringRef("Array").get(), exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    JSObjectRef arrayConstructor = JSValueToObject(ctx, arrayProperty, exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    JSObjectRef result = JSObjectCallAsConstructor(ctx, arrayConstructor, 0, 0, exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    JSValueRef pushProperty = JSObjectGetProperty(ctx, result, jsStringRef("push").get(), exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    JSObjectRef pushFunction = JSValueToObject(ctx, pushProperty, exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    Vector<String> tableNames = database->tableNames();
    unsigned length = tableNames.size();
    for (unsigned i = 0; i < length; ++i) {
        String tableName = tableNames[i];
        JSValueRef tableNameValue = JSValueMakeString(ctx, jsStringRef(tableName).get());

        JSValueRef pushArguments[] = { tableNameValue };
        JSObjectCallAsFunction(ctx, pushFunction, result, 1, pushArguments, exception);
        if (exception && *exception)
            return JSValueMakeUndefined(ctx);
    }

    return result;
}
#endif

static JSValueRef inspectedWindow(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef[] /*arguments[]*/, JSValueRef* /*exception*/)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    JSDOMWindow* inspectedWindow = toJSDOMWindow(controller->inspectedPage()->mainFrame());
    JSLock lock;
    return toRef(JSInspectedObjectWrapper::wrap(inspectedWindow->globalExec(), inspectedWindow));
}

static JSValueRef localizedStrings(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef[] /*arguments[]*/, JSValueRef* /*exception*/)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    String url = controller->localizedStringsURL();
    if (url.isNull())
        return JSValueMakeNull(ctx);

    return JSValueMakeString(ctx, jsStringRef(url).get());
}

static JSValueRef platform(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef[] /*arguments[]*/, JSValueRef* /*exception*/)
{
#if PLATFORM(MAC)
#ifdef BUILDING_ON_TIGER
    static const String platform = "mac-tiger";
#else
    static const String platform = "mac-leopard";
#endif
#elif PLATFORM(WIN_OS)
    static const String platform = "windows";
#elif PLATFORM(QT)
    static const String platform = "qt";
#elif PLATFORM(GTK)
    static const String platform = "gtk";
#elif PLATFORM(WX)
    static const String platform = "wx";
#elif PLATFORM(BAL)
    static const String platform = "bal";
#else
    static const String platform = "unknown";
#endif

    JSValueRef platformValue = JSValueMakeString(ctx, jsStringRef(platform).get());

    return platformValue;
}

static JSValueRef moveByUnrestricted(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    if (argumentCount < 2)
        return JSValueMakeUndefined(ctx);

    double x = JSValueToNumber(ctx, arguments[0], exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    double y = JSValueToNumber(ctx, arguments[1], exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    controller->moveWindowBy(narrowPrecisionToFloat(x), narrowPrecisionToFloat(y));

    return JSValueMakeUndefined(ctx);
}

static JSValueRef wrapCallback(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    if (argumentCount < 1)
        return JSValueMakeUndefined(ctx);

    JSLock lock;
    return toRef(JSInspectorCallbackWrapper::wrap(toJS(ctx), toJS(arguments[0])));
}

static JSValueRef startDebuggingAndReloadInspectedPage(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef[] /*arguments*/, JSValueRef* /*exception*/)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    controller->startDebuggingAndReloadInspectedPage();

    return JSValueMakeUndefined(ctx);
}

static JSValueRef stopDebugging(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef[] /*arguments*/, JSValueRef* /*exception*/)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    controller->stopDebugging();

    return JSValueMakeUndefined(ctx);
}

static JSValueRef debuggerAttached(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef[] /*arguments*/, JSValueRef* /*exception*/)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);
    return JSValueMakeBoolean(ctx, controller->debuggerAttached());
}

static JSValueRef currentCallFrame(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef[] /*arguments*/, JSValueRef* /*exception*/)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    JavaScriptCallFrame* callFrame = controller->currentCallFrame();
    if (!callFrame || !callFrame->isValid())
        return JSValueMakeNull(ctx);

    ExecState* globalExec = callFrame->scopeChain()->globalObject()->globalExec();

    JSLock lock;
    return toRef(JSInspectedObjectWrapper::wrap(globalExec, toJS(toJS(ctx), callFrame)));
}

static JSValueRef pauseOnExceptions(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* /*exception*/)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);
    return JSValueMakeBoolean(ctx, controller->pauseOnExceptions());
}

static JSValueRef setPauseOnExceptions(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* /*exception*/)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    if (argumentCount < 1)
        return JSValueMakeUndefined(ctx);

    controller->setPauseOnExceptions(JSValueToBoolean(ctx, arguments[0]));

    return JSValueMakeUndefined(ctx);
}

static JSValueRef pauseInDebugger(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef[] /*arguments*/, JSValueRef* /*exception*/)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    controller->pauseInDebugger();

    return JSValueMakeUndefined(ctx);
}

static JSValueRef resumeDebugger(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef[] /*arguments*/, JSValueRef* /*exception*/)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    controller->resumeDebugger();

    return JSValueMakeUndefined(ctx);
}

static JSValueRef stepOverStatementInDebugger(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef[] /*arguments*/, JSValueRef* /*exception*/)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    controller->stepOverStatementInDebugger();

    return JSValueMakeUndefined(ctx);
}

static JSValueRef stepIntoStatementInDebugger(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef[] /*arguments*/, JSValueRef* /*exception*/)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    controller->stepIntoStatementInDebugger();

    return JSValueMakeUndefined(ctx);
}

static JSValueRef stepOutOfFunctionInDebugger(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef[] /*arguments*/, JSValueRef* /*exception*/)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    controller->stepOutOfFunctionInDebugger();

    return JSValueMakeUndefined(ctx);
}

static JSValueRef addBreakpoint(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    if (argumentCount < 2)
        return JSValueMakeUndefined(ctx);

    double sourceID = JSValueToNumber(ctx, arguments[0], exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    double lineNumber = JSValueToNumber(ctx, arguments[1], exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    controller->addBreakpoint(static_cast<int>(sourceID), static_cast<unsigned>(lineNumber));

    return JSValueMakeUndefined(ctx);
}

static JSValueRef removeBreakpoint(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    if (argumentCount < 2)
        return JSValueMakeUndefined(ctx);

    double sourceID = JSValueToNumber(ctx, arguments[0], exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    double lineNumber = JSValueToNumber(ctx, arguments[1], exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    controller->removeBreakpoint(static_cast<int>(sourceID), static_cast<unsigned>(lineNumber));

    return JSValueMakeUndefined(ctx);
}

// Profiles

static JSValueRef profiles(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef[] /*arguments*/, JSValueRef* exception)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    JSLock lock;

    const Vector<RefPtr<Profile> >& profiles = controller->profiles();

    JSObjectRef global = JSContextGetGlobalObject(ctx);

    JSValueRef arrayProperty = JSObjectGetProperty(ctx, global, jsStringRef("Array").get(), exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    JSObjectRef arrayConstructor = JSValueToObject(ctx, arrayProperty, exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    JSObjectRef result = JSObjectCallAsConstructor(ctx, arrayConstructor, 0, 0, exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    JSValueRef pushProperty = JSObjectGetProperty(ctx, result, jsStringRef("push").get(), exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    JSObjectRef pushFunction = JSValueToObject(ctx, pushProperty, exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    for (size_t i = 0; i < profiles.size(); ++i) {
        JSValueRef arg0 = toRef(toJS(toJS(ctx), profiles[i].get()));
        JSObjectCallAsFunction(ctx, pushFunction, result, 1, &arg0, exception);
        if (exception && *exception)
            return JSValueMakeUndefined(ctx);
    }

    return result;
}

// InspectorController Class

InspectorController::InspectorController(Page* page, InspectorClient* client)
    : m_inspectedPage(page)
    , m_client(client)
    , m_page(0)
    , m_scriptObject(0)
    , m_controllerScriptObject(0)
    , m_scriptContext(0)
    , m_windowVisible(false)
    , m_debuggerAttached(false)
    , m_attachDebuggerWhenShown(false)
    , m_recordingUserInitiatedProfile(false)
    , m_showAfterVisible(ElementsPanel)
    , m_nextIdentifier(-2)
{
    ASSERT_ARG(page, page);
    ASSERT_ARG(client, client);
}

InspectorController::~InspectorController()
{
    m_client->inspectorDestroyed();

    if (m_scriptContext) {
        JSValueRef exception = 0;

        JSObjectRef global = JSContextGetGlobalObject(m_scriptContext);
        JSValueRef controllerProperty = JSObjectGetProperty(m_scriptContext, global, jsStringRef("InspectorController").get(), &exception);
        if (!HANDLE_EXCEPTION(m_scriptContext, exception)) {
            if (JSObjectRef controller = JSValueToObject(m_scriptContext, controllerProperty, &exception)) {
                if (!HANDLE_EXCEPTION(m_scriptContext, exception))
                    JSObjectSetPrivate(controller, 0);
            }
        }
    }

    if (m_page)
        m_page->setParentInspectorController(0);

    // m_inspectedPage should have been cleared in inspectedPageDestroyed().
    ASSERT(!m_inspectedPage);

    deleteAllValues(m_frameResources);
    deleteAllValues(m_consoleMessages);
}

void InspectorController::inspectedPageDestroyed()
{
    close();

    ASSERT(m_inspectedPage);
    m_inspectedPage = 0;
}

bool InspectorController::enabled() const
{
    if (!m_inspectedPage)
        return false;

    return m_inspectedPage->settings()->developerExtrasEnabled();
}

String InspectorController::localizedStringsURL()
{
    if (!enabled())
        return String();
    return m_client->localizedStringsURL();
}

// Trying to inspect something in a frame with JavaScript disabled would later lead to
// crashes trying to create JavaScript wrappers. Some day we could fix this issue, but
// for now prevent crashes here by never targeting a node in such a frame.
static bool canPassNodeToJavaScript(Node* node)
{
    if (!node)
        return false;
    Frame* frame = node->document()->frame();
    return frame && frame->script()->isEnabled();
}

void InspectorController::inspect(Node* node)
{
    if (!canPassNodeToJavaScript(node) || !enabled())
        return;

    show();

    if (node->nodeType() != Node::ELEMENT_NODE && node->nodeType() != Node::DOCUMENT_NODE)
        node = node->parentNode();
    m_nodeToFocus = node;

    if (!m_scriptObject) {
        m_showAfterVisible = ElementsPanel;
        return;
    }

    if (windowVisible())
        focusNode();
}

void InspectorController::focusNode()
{
    if (!enabled())
        return;

    ASSERT(m_scriptContext);
    ASSERT(m_scriptObject);
    ASSERT(m_nodeToFocus);

    Frame* frame = m_nodeToFocus->document()->frame();
    if (!frame)
        return;

    ExecState* exec = toJSDOMWindow(frame)->globalExec();

    JSValueRef arg0;

    {
        KJS::JSLock lock;
        arg0 = toRef(JSInspectedObjectWrapper::wrap(exec, toJS(exec, m_nodeToFocus.get())));
    }

    m_nodeToFocus = 0;

    JSValueRef exception = 0;
    callFunction(m_scriptContext, m_scriptObject, "updateFocusedNode", 1, &arg0, exception);
}

void InspectorController::highlight(Node* node)
{
    if (!enabled())
        return;
    ASSERT_ARG(node, node);
    m_highlightedNode = node;
    m_client->highlight(node);
}

void InspectorController::hideHighlight()
{
    if (!enabled())
        return;
    m_client->hideHighlight();
}

bool InspectorController::windowVisible()
{
    return m_windowVisible;
}

void InspectorController::setWindowVisible(bool visible)
{
    if (visible == m_windowVisible)
        return;

    m_windowVisible = visible;

    if (!m_scriptContext || !m_scriptObject)
        return;

    if (m_windowVisible) {
        populateScriptObjects();
        if (m_nodeToFocus)
            focusNode();
        if (m_attachDebuggerWhenShown)
            startDebuggingAndReloadInspectedPage();
        if (m_showAfterVisible != CurrentPanel)
            showPanel(m_showAfterVisible);
    } else
        resetScriptObjects();

    m_showAfterVisible = CurrentPanel;
}

void InspectorController::addMessageToConsole(MessageSource source, MessageLevel level, ExecState* exec, const ArgList& arguments, unsigned lineNumber, const String& sourceURL)
{
    if (!enabled())
        return;

    addConsoleMessage(new ConsoleMessage(source, level, exec, arguments, lineNumber, sourceURL));
}

void InspectorController::addMessageToConsole(MessageSource source, MessageLevel level, const String& message, unsigned lineNumber, const String& sourceID)
{
    if (!enabled())
        return;

    addConsoleMessage(new ConsoleMessage(source, level, message, lineNumber, sourceID));
}

void InspectorController::addConsoleMessage(ConsoleMessage* consoleMessage)
{
    ASSERT(enabled());
    ASSERT_ARG(consoleMessage, consoleMessage);

    m_consoleMessages.append(consoleMessage);

    if (windowVisible())
        addScriptConsoleMessage(consoleMessage);
}

void InspectorController::addProfile(PassRefPtr<Profile> prpProfile)
{
    if (!enabled())
        return;

    RefPtr<Profile> profile = prpProfile;
    m_profiles.append(profile);

    if (windowVisible())
        addScriptProfile(profile.get());
}

void InspectorController::attachWindow()
{
    if (!enabled())
        return;
    m_client->attachWindow();
}

void InspectorController::detachWindow()
{
    if (!enabled())
        return;
    m_client->detachWindow();
}

void InspectorController::windowScriptObjectAvailable()
{
    if (!m_page || !enabled())
        return;

    m_scriptContext = toRef(m_page->mainFrame()->script()->globalObject()->globalExec());

    JSObjectRef global = JSContextGetGlobalObject(m_scriptContext);
    ASSERT(global);

    static JSStaticFunction staticFunctions[] = {
        { "addResourceSourceToFrame", addResourceSourceToFrame, kJSPropertyAttributeNone },
        { "addSourceToFrame", addSourceToFrame, kJSPropertyAttributeNone },
        { "getResourceDocumentNode", getResourceDocumentNode, kJSPropertyAttributeNone },
        { "highlightDOMNode", highlightDOMNode, kJSPropertyAttributeNone },
        { "hideDOMNodeHighlight", hideDOMNodeHighlight, kJSPropertyAttributeNone },
        { "loaded", loaded, kJSPropertyAttributeNone },
        { "windowUnloading", unloading, kJSPropertyAttributeNone },
        { "attach", attach, kJSPropertyAttributeNone },
        { "detach", detach, kJSPropertyAttributeNone },
        { "search", search, kJSPropertyAttributeNone },
#if ENABLE(DATABASE)
        { "databaseTableNames", databaseTableNames, kJSPropertyAttributeNone },
#endif
        { "inspectedWindow", inspectedWindow, kJSPropertyAttributeNone },
        { "localizedStringsURL", localizedStrings, kJSPropertyAttributeNone },
        { "platform", platform, kJSPropertyAttributeNone },
        { "moveByUnrestricted", moveByUnrestricted, kJSPropertyAttributeNone },
        { "wrapCallback", wrapCallback, kJSPropertyAttributeNone },
        { "startDebuggingAndReloadInspectedPage", WebCore::startDebuggingAndReloadInspectedPage, kJSPropertyAttributeNone },
        { "stopDebugging", WebCore::stopDebugging, kJSPropertyAttributeNone },
        { "debuggerAttached", WebCore::debuggerAttached, kJSPropertyAttributeNone },
        { "profiles", WebCore::profiles, kJSPropertyAttributeNone },
        { "currentCallFrame", WebCore::currentCallFrame, kJSPropertyAttributeNone },
        { "pauseOnExceptions", WebCore::pauseOnExceptions, kJSPropertyAttributeNone },
        { "setPauseOnExceptions", WebCore::setPauseOnExceptions, kJSPropertyAttributeNone },
        { "pauseInDebugger", WebCore::pauseInDebugger, kJSPropertyAttributeNone },
        { "resumeDebugger", WebCore::resumeDebugger, kJSPropertyAttributeNone },
        { "stepOverStatementInDebugger", WebCore::stepOverStatementInDebugger, kJSPropertyAttributeNone },
        { "stepIntoStatementInDebugger", WebCore::stepIntoStatementInDebugger, kJSPropertyAttributeNone },
        { "stepOutOfFunctionInDebugger", WebCore::stepOutOfFunctionInDebugger, kJSPropertyAttributeNone },
        { "addBreakpoint", WebCore::addBreakpoint, kJSPropertyAttributeNone },
        { "removeBreakpoint", WebCore::removeBreakpoint, kJSPropertyAttributeNone },
        { 0, 0, 0 }
    };

    JSClassDefinition inspectorControllerDefinition = {
        0, kJSClassAttributeNone, "InspectorController", 0, 0, staticFunctions,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    JSClassRef controllerClass = JSClassCreate(&inspectorControllerDefinition);
    ASSERT(controllerClass);

    m_controllerScriptObject = JSObjectMake(m_scriptContext, controllerClass, reinterpret_cast<void*>(this));
    ASSERT(m_controllerScriptObject);

    JSObjectSetProperty(m_scriptContext, global, jsStringRef("InspectorController").get(), m_controllerScriptObject, kJSPropertyAttributeNone, 0);
}

void InspectorController::scriptObjectReady()
{
    ASSERT(m_scriptContext);
    if (!m_scriptContext)
        return;

    JSObjectRef global = JSContextGetGlobalObject(m_scriptContext);
    ASSERT(global);

    JSValueRef exception = 0;

    JSValueRef inspectorValue = JSObjectGetProperty(m_scriptContext, global, jsStringRef("WebInspector").get(), &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return;

    ASSERT(inspectorValue);
    if (!inspectorValue)
        return;

    m_scriptObject = JSValueToObject(m_scriptContext, inspectorValue, &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return;

    ASSERT(m_scriptObject);

    JSValueProtect(m_scriptContext, m_scriptObject);

    // Make sure our window is visible now that the page loaded
    showWindow();
}

void InspectorController::show()
{
    if (!enabled())
        return;

    if (!m_page) {
        m_page = m_client->createPage();
        if (!m_page)
            return;
        m_page->setParentInspectorController(this);

        // showWindow() will be called after the page loads in scriptObjectReady()
        return;
    }

    showWindow();
}

void InspectorController::showPanel(SpecialPanels panel)
{
    if (!enabled())
        return;

    show();

    if (!m_scriptObject) {
        m_showAfterVisible = panel;
        return;
    }

    if (panel == CurrentPanel)
        return;

    const char* showFunctionName;
    switch (panel) {
        case ConsolePanel:
            showFunctionName = "showConsole";
            break;
        case DatabasesPanel:
            showFunctionName = "showDatabasesPanel";
            break;
        case ElementsPanel:
            showFunctionName = "showElementsPanel";
            break;
        case ProfilesPanel:
            showFunctionName = "showProfilesPanel";
            break;
        case ResourcesPanel:
            showFunctionName = "showResourcesPanel";
            break;
        case ScriptsPanel:
            showFunctionName = "showScriptsPanel";
            break;
        default:
            ASSERT_NOT_REACHED();
            showFunctionName = 0;
    }

    if (showFunctionName)
        callSimpleFunction(m_scriptContext, m_scriptObject, showFunctionName);
}

void InspectorController::close()
{
    if (!enabled())
        return;

    stopUserInitiatedProfiling();
    stopDebugging();
    closeWindow();

    if (m_scriptContext && m_scriptObject)
        JSValueUnprotect(m_scriptContext, m_scriptObject);

    m_scriptObject = 0;
    m_scriptContext = 0;
}

void InspectorController::showWindow()
{
    ASSERT(enabled());
    m_client->showWindow();
}

void InspectorController::closeWindow()
{
    m_client->closeWindow();
}

void InspectorController::startUserInitiatedProfiling()
{
    if (!enabled())
        return;

    m_recordingUserInitiatedProfile = true;

    ExecState* exec = toJSDOMWindow(m_inspectedPage->mainFrame())->globalExec();
    Profiler::profiler()->startProfiling(exec, UserInitiatedProfileName, this);
}

void InspectorController::stopUserInitiatedProfiling()
{
    if (!enabled())
        return;

    m_recordingUserInitiatedProfile = false;

    ExecState* exec = toJSDOMWindow(m_inspectedPage->mainFrame())->globalExec();
    Profiler::profiler()->stopProfiling(exec, UserInitiatedProfileName);
}

void InspectorController::finishedProfiling(PassRefPtr<Profile> prpProfile)
{
    addProfile(prpProfile);
}

static void addHeaders(JSContextRef context, JSObjectRef object, const HTTPHeaderMap& headers, JSValueRef* exception)
{
    ASSERT_ARG(context, context);
    ASSERT_ARG(object, object);

    HTTPHeaderMap::const_iterator end = headers.end();
    for (HTTPHeaderMap::const_iterator it = headers.begin(); it != end; ++it) {
        JSValueRef value = JSValueMakeString(context, jsStringRef(it->second).get());
        JSObjectSetProperty(context, object, jsStringRef(it->first).get(), value, kJSPropertyAttributeNone, exception);
        if (exception && *exception)
            return;
    }
}

static JSObjectRef scriptObjectForRequest(JSContextRef context, const InspectorResource* resource, JSValueRef* exception)
{
    ASSERT_ARG(context, context);

    JSObjectRef object = JSObjectMake(context, 0, 0);
    addHeaders(context, object, resource->requestHeaderFields, exception);

    return object;
}

static JSObjectRef scriptObjectForResponse(JSContextRef context, const InspectorResource* resource, JSValueRef* exception)
{
    ASSERT_ARG(context, context);

    JSObjectRef object = JSObjectMake(context, 0, 0);
    addHeaders(context, object, resource->responseHeaderFields, exception);

    return object;
}

JSObjectRef InspectorController::addScriptResource(InspectorResource* resource)
{
    ASSERT_ARG(resource, resource);

    ASSERT(m_scriptContext);
    ASSERT(m_scriptObject);
    if (!m_scriptContext || !m_scriptObject)
        return 0;

    if (!resource->scriptObject) {
        JSValueRef exception = 0;

        JSValueRef resourceProperty = JSObjectGetProperty(m_scriptContext, m_scriptObject, jsStringRef("Resource").get(), &exception);
        if (HANDLE_EXCEPTION(m_scriptContext, exception))
            return 0;

        JSObjectRef resourceConstructor = JSValueToObject(m_scriptContext, resourceProperty, &exception);
        if (HANDLE_EXCEPTION(m_scriptContext, exception))
            return 0;

        JSValueRef urlValue = JSValueMakeString(m_scriptContext, jsStringRef(resource->requestURL.string()).get());
        JSValueRef domainValue = JSValueMakeString(m_scriptContext, jsStringRef(resource->requestURL.host()).get());
        JSValueRef pathValue = JSValueMakeString(m_scriptContext, jsStringRef(resource->requestURL.path()).get());
        JSValueRef lastPathComponentValue = JSValueMakeString(m_scriptContext, jsStringRef(resource->requestURL.lastPathComponent()).get());

        JSValueRef identifier = JSValueMakeNumber(m_scriptContext, resource->identifier);
        JSValueRef mainResource = JSValueMakeBoolean(m_scriptContext, m_mainResource == resource);
        JSValueRef cached = JSValueMakeBoolean(m_scriptContext, resource->cached);

        JSObjectRef scriptObject = scriptObjectForRequest(m_scriptContext, resource, &exception);
        if (HANDLE_EXCEPTION(m_scriptContext, exception))
            return 0;

        JSValueRef arguments[] = { scriptObject, urlValue, domainValue, pathValue, lastPathComponentValue, identifier, mainResource, cached };
        JSObjectRef result = JSObjectCallAsConstructor(m_scriptContext, resourceConstructor, 8, arguments, &exception);
        if (HANDLE_EXCEPTION(m_scriptContext, exception))
            return 0;

        ASSERT(result);

        resource->setScriptObject(m_scriptContext, result);
    }

    JSValueRef exception = 0;
    callFunction(m_scriptContext, m_scriptObject, "addResource", 1, &resource->scriptObject, exception);

    if (exception)
        return 0;

    return resource->scriptObject;
}

JSObjectRef InspectorController::addAndUpdateScriptResource(InspectorResource* resource)
{
    ASSERT_ARG(resource, resource);

    JSObjectRef scriptResource = addScriptResource(resource);
    if (!scriptResource)
        return 0;

    updateScriptResourceResponse(resource);
    updateScriptResource(resource, resource->length);
    updateScriptResource(resource, resource->startTime, resource->responseReceivedTime, resource->endTime);
    updateScriptResource(resource, resource->finished, resource->failed);
    return scriptResource;
}

void InspectorController::removeScriptResource(InspectorResource* resource)
{
    ASSERT(m_scriptContext);
    ASSERT(m_scriptObject);
    if (!m_scriptContext || !m_scriptObject)
        return;

    ASSERT(resource);
    ASSERT(resource->scriptObject);
    if (!resource || !resource->scriptObject)
        return;

    JSObjectRef scriptObject = resource->scriptObject;
    resource->setScriptObject(0, 0);

    JSValueRef exception = 0;
    callFunction(m_scriptContext, m_scriptObject, "removeResource", 1, &scriptObject, exception);
}

static void updateResourceRequest(InspectorResource* resource, const ResourceRequest& request)
{
    resource->requestHeaderFields = request.httpHeaderFields();
    resource->requestURL = request.url();
}

static void updateResourceResponse(InspectorResource* resource, const ResourceResponse& response)
{
    resource->expectedContentLength = response.expectedContentLength();
    resource->mimeType = response.mimeType();
    resource->responseHeaderFields = response.httpHeaderFields();
    resource->responseStatusCode = response.httpStatusCode();
    resource->suggestedFilename = response.suggestedFilename();
}

void InspectorController::updateScriptResourceRequest(InspectorResource* resource)
{
    ASSERT(resource->scriptObject);
    ASSERT(m_scriptContext);
    if (!resource->scriptObject || !m_scriptContext)
        return;

    JSValueRef urlValue = JSValueMakeString(m_scriptContext, jsStringRef(resource->requestURL.string()).get());
    JSValueRef domainValue = JSValueMakeString(m_scriptContext, jsStringRef(resource->requestURL.host()).get());
    JSValueRef pathValue = JSValueMakeString(m_scriptContext, jsStringRef(resource->requestURL.path()).get());
    JSValueRef lastPathComponentValue = JSValueMakeString(m_scriptContext, jsStringRef(resource->requestURL.lastPathComponent()).get());

    JSValueRef mainResourceValue = JSValueMakeBoolean(m_scriptContext, m_mainResource == resource);

    JSValueRef exception = 0;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("url").get(), urlValue, kJSPropertyAttributeNone, &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("domain").get(), domainValue, kJSPropertyAttributeNone, &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("path").get(), pathValue, kJSPropertyAttributeNone, &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("lastPathComponent").get(), lastPathComponentValue, kJSPropertyAttributeNone, &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return;

    JSObjectRef scriptObject = scriptObjectForRequest(m_scriptContext, resource, &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("requestHeaders").get(), scriptObject, kJSPropertyAttributeNone, &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("mainResource").get(), mainResourceValue, kJSPropertyAttributeNone, &exception);
    HANDLE_EXCEPTION(m_scriptContext, exception);
}

void InspectorController::updateScriptResourceResponse(InspectorResource* resource)
{
    ASSERT(resource->scriptObject);
    ASSERT(m_scriptContext);
    if (!resource->scriptObject || !m_scriptContext)
        return;

    JSValueRef mimeTypeValue = JSValueMakeString(m_scriptContext, jsStringRef(resource->mimeType).get());

    JSValueRef suggestedFilenameValue = JSValueMakeString(m_scriptContext, jsStringRef(resource->suggestedFilename).get());

    JSValueRef expectedContentLengthValue = JSValueMakeNumber(m_scriptContext, static_cast<double>(resource->expectedContentLength));
    JSValueRef statusCodeValue = JSValueMakeNumber(m_scriptContext, resource->responseStatusCode);

    JSValueRef exception = 0;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("mimeType").get(), mimeTypeValue, kJSPropertyAttributeNone, &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("suggestedFilename").get(), suggestedFilenameValue, kJSPropertyAttributeNone, &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("expectedContentLength").get(), expectedContentLengthValue, kJSPropertyAttributeNone, &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("statusCode").get(), statusCodeValue, kJSPropertyAttributeNone, &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return;

    JSObjectRef scriptObject = scriptObjectForResponse(m_scriptContext, resource, &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("responseHeaders").get(), scriptObject, kJSPropertyAttributeNone, &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return;

    updateScriptResourceType(resource);
}

void InspectorController::updateScriptResourceType(InspectorResource* resource)
{
    ASSERT(resource->scriptObject);
    ASSERT(m_scriptContext);
    if (!resource->scriptObject || !m_scriptContext)
        return;

    JSValueRef exception = 0;

    JSValueRef typeValue = JSValueMakeNumber(m_scriptContext, resource->type());
    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("type").get(), typeValue, kJSPropertyAttributeNone, &exception);
    HANDLE_EXCEPTION(m_scriptContext, exception);
}

void InspectorController::updateScriptResource(InspectorResource* resource, int length)
{
    ASSERT(resource->scriptObject);
    ASSERT(m_scriptContext);
    if (!resource->scriptObject || !m_scriptContext)
        return;

    JSValueRef lengthValue = JSValueMakeNumber(m_scriptContext, length);

    JSValueRef exception = 0;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("contentLength").get(), lengthValue, kJSPropertyAttributeNone, &exception);
    HANDLE_EXCEPTION(m_scriptContext, exception);
}

void InspectorController::updateScriptResource(InspectorResource* resource, bool finished, bool failed)
{
    ASSERT(resource->scriptObject);
    ASSERT(m_scriptContext);
    if (!resource->scriptObject || !m_scriptContext)
        return;

    JSValueRef failedValue = JSValueMakeBoolean(m_scriptContext, failed);
    JSValueRef finishedValue = JSValueMakeBoolean(m_scriptContext, finished);

    JSValueRef exception = 0;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("failed").get(), failedValue, kJSPropertyAttributeNone, &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("finished").get(), finishedValue, kJSPropertyAttributeNone, &exception);
    HANDLE_EXCEPTION(m_scriptContext, exception);
}

void InspectorController::updateScriptResource(InspectorResource* resource, double startTime, double responseReceivedTime, double endTime)
{
    ASSERT(resource->scriptObject);
    ASSERT(m_scriptContext);
    if (!resource->scriptObject || !m_scriptContext)
        return;

    JSValueRef startTimeValue = JSValueMakeNumber(m_scriptContext, startTime);
    JSValueRef responseReceivedTimeValue = JSValueMakeNumber(m_scriptContext, responseReceivedTime);
    JSValueRef endTimeValue = JSValueMakeNumber(m_scriptContext, endTime);

    JSValueRef exception = 0;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("startTime").get(), startTimeValue, kJSPropertyAttributeNone, &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("responseReceivedTime").get(), responseReceivedTimeValue, kJSPropertyAttributeNone, &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("endTime").get(), endTimeValue, kJSPropertyAttributeNone, &exception);
    HANDLE_EXCEPTION(m_scriptContext, exception);
}

void InspectorController::populateScriptObjects()
{
    ASSERT(m_scriptContext);
    if (!m_scriptContext)
        return;

    ResourcesMap::iterator resourcesEnd = m_resources.end();
    for (ResourcesMap::iterator it = m_resources.begin(); it != resourcesEnd; ++it)
        addAndUpdateScriptResource(it->second.get());

    unsigned messageCount = m_consoleMessages.size();
    for (unsigned i = 0; i < messageCount; ++i)
        addScriptConsoleMessage(m_consoleMessages[i]);

#if ENABLE(DATABASE)
    DatabaseResourcesSet::iterator databasesEnd = m_databaseResources.end();
    for (DatabaseResourcesSet::iterator it = m_databaseResources.begin(); it != databasesEnd; ++it)
        addDatabaseScriptResource((*it).get());
#endif

    callSimpleFunction(m_scriptContext, m_scriptObject, "populateInterface");
}

#if ENABLE(DATABASE)
JSObjectRef InspectorController::addDatabaseScriptResource(InspectorDatabaseResource* resource)
{
    ASSERT_ARG(resource, resource);

    if (resource->scriptObject)
        return resource->scriptObject;

    ASSERT(m_scriptContext);
    ASSERT(m_scriptObject);
    if (!m_scriptContext || !m_scriptObject)
        return 0;

    Frame* frame = resource->database->document()->frame();
    if (!frame)
        return 0;

    JSValueRef exception = 0;

    JSValueRef databaseProperty = JSObjectGetProperty(m_scriptContext, m_scriptObject, jsStringRef("Database").get(), &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return 0;

    JSObjectRef databaseConstructor = JSValueToObject(m_scriptContext, databaseProperty, &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return 0;

    ExecState* exec = toJSDOMWindow(frame)->globalExec();

    JSValueRef database;

    {
        KJS::JSLock lock;
        database = toRef(JSInspectedObjectWrapper::wrap(exec, toJS(exec, resource->database.get())));
    }

    JSValueRef domainValue = JSValueMakeString(m_scriptContext, jsStringRef(resource->domain).get());
    JSValueRef nameValue = JSValueMakeString(m_scriptContext, jsStringRef(resource->name).get());
    JSValueRef versionValue = JSValueMakeString(m_scriptContext, jsStringRef(resource->version).get());

    JSValueRef arguments[] = { database, domainValue, nameValue, versionValue };
    JSObjectRef result = JSObjectCallAsConstructor(m_scriptContext, databaseConstructor, 4, arguments, &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return 0;

    ASSERT(result);

    callFunction(m_scriptContext, m_scriptObject, "addDatabase", 1, &result, exception);

    if (exception)
        return 0;

    resource->setScriptObject(m_scriptContext, result);

    return result;
}

void InspectorController::removeDatabaseScriptResource(InspectorDatabaseResource* resource)
{
    ASSERT(m_scriptContext);
    ASSERT(m_scriptObject);
    if (!m_scriptContext || !m_scriptObject)
        return;

    ASSERT(resource);
    ASSERT(resource->scriptObject);
    if (!resource || !resource->scriptObject)
        return;

    JSObjectRef scriptObject = resource->scriptObject;
    resource->setScriptObject(0, 0);

    JSValueRef exception = 0;
    callFunction(m_scriptContext, m_scriptObject, "removeDatabase", 1, &scriptObject, exception);
}
#endif

void InspectorController::addScriptConsoleMessage(const ConsoleMessage* message)
{
    ASSERT_ARG(message, message);

    JSValueRef exception = 0;

    JSValueRef messageConstructorProperty = JSObjectGetProperty(m_scriptContext, m_scriptObject, jsStringRef("ConsoleMessage").get(), &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return;

    JSObjectRef messageConstructor = JSValueToObject(m_scriptContext, messageConstructorProperty, &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return;

    JSValueRef sourceValue = JSValueMakeNumber(m_scriptContext, message->source);
    JSValueRef levelValue = JSValueMakeNumber(m_scriptContext, message->level);
    JSValueRef lineValue = JSValueMakeNumber(m_scriptContext, message->line);
    JSValueRef urlValue = JSValueMakeString(m_scriptContext, jsStringRef(message->url).get());

    static const unsigned maximumMessageArguments = 256;
    JSValueRef arguments[maximumMessageArguments];
    unsigned argumentCount = 0;
    arguments[argumentCount++] = sourceValue;
    arguments[argumentCount++] = levelValue;
    arguments[argumentCount++] = lineValue;
    arguments[argumentCount++] = urlValue;

    if (!message->wrappedArguments.isEmpty()) {
        unsigned remainingSpaceInArguments = maximumMessageArguments - argumentCount;
        unsigned argumentsToAdd = min(remainingSpaceInArguments, static_cast<unsigned>(message->wrappedArguments.size()));
        for (unsigned i = 0; i < argumentsToAdd; ++i)
            arguments[argumentCount++] = toRef(message->wrappedArguments[i]);
    } else {
        JSValueRef messageValue = JSValueMakeString(m_scriptContext, jsStringRef(message->message).get());
        arguments[argumentCount++] = messageValue;
    }

    JSObjectRef messageObject = JSObjectCallAsConstructor(m_scriptContext, messageConstructor, argumentCount, arguments, &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return;

    callFunction(m_scriptContext, m_scriptObject, "addMessageToConsole", 1, &messageObject, exception);
}

void InspectorController::addScriptProfile(Profile* profile)
{
    JSLock lock;
    JSValueRef exception = 0;
    JSValueRef profileObject = toRef(toJS(toJS(m_scriptContext), profile));
    callFunction(m_scriptContext, m_scriptObject, "addProfile", 1, &profileObject, exception);
}

void InspectorController::resetScriptObjects()
{
    if (!m_scriptContext || !m_scriptObject)
        return;

    ResourcesMap::iterator resourcesEnd = m_resources.end();
    for (ResourcesMap::iterator it = m_resources.begin(); it != resourcesEnd; ++it) {
        InspectorResource* resource = it->second.get();
        resource->setScriptObject(0, 0);
    }

#if ENABLE(DATABASE)
    DatabaseResourcesSet::iterator databasesEnd = m_databaseResources.end();
    for (DatabaseResourcesSet::iterator it = m_databaseResources.begin(); it != databasesEnd; ++it) {
        InspectorDatabaseResource* resource = (*it).get();
        resource->setScriptObject(0, 0);
    }
#endif

    callSimpleFunction(m_scriptContext, m_scriptObject, "reset");
}

void InspectorController::pruneResources(ResourcesMap* resourceMap, DocumentLoader* loaderToKeep)
{
    ASSERT_ARG(resourceMap, resourceMap);

    ResourcesMap mapCopy(*resourceMap);
    ResourcesMap::iterator end = mapCopy.end();
    for (ResourcesMap::iterator it = mapCopy.begin(); it != end; ++it) {
        InspectorResource* resource = (*it).second.get();
        if (resource == m_mainResource)
            continue;

        if (!loaderToKeep || resource->loader != loaderToKeep) {
            removeResource(resource);
            if (windowVisible() && resource->scriptObject)
                removeScriptResource(resource);
        }
    }
}

void InspectorController::didCommitLoad(DocumentLoader* loader)
{
    if (!enabled())
        return;

    ASSERT(m_inspectedPage);

    if (loader->frame() == m_inspectedPage->mainFrame()) {
        m_client->inspectedURLChanged(loader->url().string());

        deleteAllValues(m_consoleMessages);
        m_consoleMessages.clear();

        m_profiles.clear();

#if ENABLE(DATABASE)
        m_databaseResources.clear();
#endif

        if (windowVisible()) {
            resetScriptObjects();

            if (!loader->isLoadingFromCachedPage()) {
                ASSERT(m_mainResource && m_mainResource->loader == loader);
                // We don't add the main resource until its load is committed. This is
                // needed to keep the load for a user-entered URL from showing up in the
                // list of resources for the page they are navigating away from.
                addAndUpdateScriptResource(m_mainResource.get());
            } else {
                // Pages loaded from the page cache are committed before
                // m_mainResource is the right resource for this load, so we
                // clear it here. It will be re-assigned in
                // identifierForInitialRequest.
                m_mainResource = 0;
            }
        }
    }

    for (Frame* frame = loader->frame(); frame; frame = frame->tree()->traverseNext(loader->frame()))
        if (ResourcesMap* resourceMap = m_frameResources.get(frame))
            pruneResources(resourceMap, loader);
}

void InspectorController::frameDetachedFromParent(Frame* frame)
{
    if (!enabled())
        return;
    if (ResourcesMap* resourceMap = m_frameResources.get(frame))
        removeAllResources(resourceMap);
}

void InspectorController::addResource(InspectorResource* resource)
{
    m_resources.set(resource->identifier, resource);
    m_knownResources.add(resource->requestURL.string());

    Frame* frame = resource->frame.get();
    ResourcesMap* resourceMap = m_frameResources.get(frame);
    if (resourceMap)
        resourceMap->set(resource->identifier, resource);
    else {
        resourceMap = new ResourcesMap;
        resourceMap->set(resource->identifier, resource);
        m_frameResources.set(frame, resourceMap);
    }
}

void InspectorController::removeResource(InspectorResource* resource)
{
    m_resources.remove(resource->identifier);
    m_knownResources.remove(resource->requestURL.string());

    Frame* frame = resource->frame.get();
    ResourcesMap* resourceMap = m_frameResources.get(frame);
    if (!resourceMap) {
        ASSERT_NOT_REACHED();
        return;
    }

    resourceMap->remove(resource->identifier);
    if (resourceMap->isEmpty()) {
        m_frameResources.remove(frame);
        delete resourceMap;
    }
}

void InspectorController::didLoadResourceFromMemoryCache(DocumentLoader* loader, const ResourceRequest& request, const ResourceResponse& response, int length)
{
    if (!enabled())
        return;

    // If the resource URL is already known, we don't need to add it again since this is just a cached load.
    if (m_knownResources.contains(request.url().string()))
        return;

    RefPtr<InspectorResource> resource = InspectorResource::create(m_nextIdentifier--, loader, loader->frame());
    resource->finished = true;

    updateResourceRequest(resource.get(), request);
    updateResourceResponse(resource.get(), response);

    resource->length = length;
    resource->cached = true;
    resource->startTime = currentTime();
    resource->responseReceivedTime = resource->startTime;
    resource->endTime = resource->startTime;

    ASSERT(m_inspectedPage);

    if (loader->frame() == m_inspectedPage->mainFrame() && request.url() == loader->requestURL())
        m_mainResource = resource;

    addResource(resource.get());

    if (windowVisible())
        addAndUpdateScriptResource(resource.get());
}

void InspectorController::identifierForInitialRequest(unsigned long identifier, DocumentLoader* loader, const ResourceRequest& request)
{
    if (!enabled())
        return;

    RefPtr<InspectorResource> resource = InspectorResource::create(identifier, loader, loader->frame());

    updateResourceRequest(resource.get(), request);

    ASSERT(m_inspectedPage);

    if (loader->frame() == m_inspectedPage->mainFrame() && request.url() == loader->requestURL())
        m_mainResource = resource;

    addResource(resource.get());

    if (windowVisible() && loader->isLoadingFromCachedPage() && resource == m_mainResource)
        addAndUpdateScriptResource(resource.get());
}

void InspectorController::willSendRequest(DocumentLoader* loader, unsigned long identifier, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    if (!enabled())
        return;

    InspectorResource* resource = m_resources.get(identifier).get();
    if (!resource)
        return;

    resource->startTime = currentTime();

    if (!redirectResponse.isNull()) {
        updateResourceRequest(resource, request);
        updateResourceResponse(resource, redirectResponse);
    }

    if (resource != m_mainResource && windowVisible()) {
        if (!resource->scriptObject)
            addScriptResource(resource);
        else
            updateScriptResourceRequest(resource);

        updateScriptResource(resource, resource->startTime, resource->responseReceivedTime, resource->endTime);

        if (!redirectResponse.isNull())
            updateScriptResourceResponse(resource);
    }
}

void InspectorController::didReceiveResponse(DocumentLoader*, unsigned long identifier, const ResourceResponse& response)
{
    if (!enabled())
        return;

    InspectorResource* resource = m_resources.get(identifier).get();
    if (!resource)
        return;

    updateResourceResponse(resource, response);

    resource->responseReceivedTime = currentTime();

    if (windowVisible() && resource->scriptObject) {
        updateScriptResourceResponse(resource);
        updateScriptResource(resource, resource->startTime, resource->responseReceivedTime, resource->endTime);
    }
}

void InspectorController::didReceiveContentLength(DocumentLoader*, unsigned long identifier, int lengthReceived)
{
    if (!enabled())
        return;

    InspectorResource* resource = m_resources.get(identifier).get();
    if (!resource)
        return;

    resource->length += lengthReceived;

    if (windowVisible() && resource->scriptObject)
        updateScriptResource(resource, resource->length);
}

void InspectorController::didFinishLoading(DocumentLoader* loader, unsigned long identifier)
{
    if (!enabled())
        return;

    RefPtr<InspectorResource> resource = m_resources.get(identifier);
    if (!resource)
        return;

    removeResource(resource.get());

    resource->finished = true;
    resource->endTime = currentTime();



    //+ 12/04/09 CSidhall -
    // With pages running Ajax with frequent HttpRequests, we can build up significant runtime memory leaks caused by non cached InspectorResources that 
    // just keep getting added to the m_resources list.  Seems that WebKit relies normally on a prune to resolve these leaks but the prune is only called 
    // on exit or with a page load.  So to fix this runtime leak, we only add the resource back to the list if it was not generated from a xmlHttpRequest 
    // or if it is a cached resource.
    // Another option could be to place the resouce on a short destruct waiting list like what the page cache does but it is unclear if that would give us 
    // any extra safety.
    // Old code:
    //  addResource(resource.get());
    //  if (windowVisible() && resource->scriptObject) {
    //      updateScriptResource(resource.get(), resource->startTime, resource->responseReceivedTime, resource->endTime);
    //      updateScriptResource(resource.get(), resource->finished);
    //  }
    //
    // New code:

    bool removed = false;
    if( (resource->type() != InspectorResource::XHR) || (resource->cached == true) )
        addResource(resource.get());
    else
        removed = true;

    if (windowVisible() && resource->scriptObject) {
        updateScriptResource(resource.get(), resource->startTime, resource->responseReceivedTime, resource->endTime);
        updateScriptResource(resource.get(), resource->finished);

        if(removed)  
            removeScriptResource(resource.get());
    }
    //-CS

}

void InspectorController::didFailLoading(DocumentLoader* loader, unsigned long identifier, const ResourceError& /*error*/)
{
    if (!enabled())
        return;

    RefPtr<InspectorResource> resource = m_resources.get(identifier);
    if (!resource)
        return;

    removeResource(resource.get());

    resource->finished = true;
    resource->failed = true;
    resource->endTime = currentTime();

    //+ 12/4/09 CSidhall - Leak fix for Ajax scripts. 
    // Old code:
    //  addResource(resource.get());
    //  if (windowVisible() && resource->scriptObject) {
    //    updateScriptResource(resource.get(), resource->startTime, resource->responseReceivedTime, resource->endTime);
    //    updateScriptResource(resource.get(), resource->finished, resource->failed);
    //  }

    // New code:
    bool removed = false;
    if( (resource->type() != InspectorResource::XHR) || (resource->cached == true) )
        addResource(resource.get());
    else
        removed = true;

    if (windowVisible() && resource->scriptObject) {
        updateScriptResource(resource.get(), resource->startTime, resource->responseReceivedTime, resource->endTime);
        updateScriptResource(resource.get(), resource->finished, resource->failed);
         
        if(removed)  
            removeScriptResource(resource.get());
    }
    //-CS
}

void InspectorController::resourceRetrievedByXMLHttpRequest(unsigned long identifier, KJS::UString& sourceString)
{
    if (!enabled())
        return;

    InspectorResource* resource = m_resources.get(identifier).get();
    if (!resource)
        return;

    resource->setXMLHttpRequestProperties(sourceString);

    if (windowVisible() && resource->scriptObject)
        updateScriptResourceType(resource);
}


#if ENABLE(DATABASE)
void InspectorController::didOpenDatabase(Database* database, const String& domain, const String& name, const String& version)
{
    if (!enabled())
        return;

    RefPtr<InspectorDatabaseResource> resource = InspectorDatabaseResource::create(database, domain, name, version);

    m_databaseResources.add(resource);

    if (windowVisible())
        addDatabaseScriptResource(resource.get());
}
#endif

void InspectorController::moveWindowBy(float x, float y) const
{
    if (!m_page || !enabled())
        return;

    FloatRect frameRect = m_page->chrome()->windowRect();
    frameRect.move(x, y);
    m_page->chrome()->setWindowRect(frameRect);
}

void InspectorController::startDebuggingAndReloadInspectedPage()
{
    if (!enabled())
        return;

    if (!m_scriptContext || !m_scriptObject) {
        m_attachDebuggerWhenShown = true;
        return;
    }

    ASSERT(m_inspectedPage);

    JavaScriptDebugServer::shared().addListener(this, m_inspectedPage);
    JavaScriptDebugServer::shared().clearBreakpoints();

    m_debuggerAttached = true;
    m_attachDebuggerWhenShown = false;

    callSimpleFunction(m_scriptContext, m_scriptObject, "debuggerAttached");

    m_inspectedPage->mainFrame()->loader()->reload();
}

void InspectorController::stopDebugging()
{
    if (!enabled())
        return;

    ASSERT(m_inspectedPage);

    JavaScriptDebugServer::shared().removeListener(this, m_inspectedPage);
    m_debuggerAttached = false;

    if (m_scriptContext && m_scriptObject)
        callSimpleFunction(m_scriptContext, m_scriptObject, "debuggerDetached");
}

JavaScriptCallFrame* InspectorController::currentCallFrame() const
{
    return JavaScriptDebugServer::shared().currentCallFrame();
}

bool InspectorController::pauseOnExceptions()
{
    return JavaScriptDebugServer::shared().pauseOnExceptions();
}

void InspectorController::setPauseOnExceptions(bool pause)
{
    JavaScriptDebugServer::shared().setPauseOnExceptions(pause);
}

void InspectorController::pauseInDebugger()
{
    if (!m_debuggerAttached)
        return;
    JavaScriptDebugServer::shared().pauseProgram();
}

void InspectorController::resumeDebugger()
{
    if (!m_debuggerAttached)
        return;
    JavaScriptDebugServer::shared().continueProgram();
}

void InspectorController::stepOverStatementInDebugger()
{
    if (!m_debuggerAttached)
        return;
    JavaScriptDebugServer::shared().stepOverStatement();
}

void InspectorController::stepIntoStatementInDebugger()
{
    if (!m_debuggerAttached)
        return;
    JavaScriptDebugServer::shared().stepIntoStatement();
}

void InspectorController::stepOutOfFunctionInDebugger()
{
    if (!m_debuggerAttached)
        return;
    JavaScriptDebugServer::shared().stepOutOfFunction();
}

void InspectorController::addBreakpoint(int sourceID, unsigned lineNumber)
{
    JavaScriptDebugServer::shared().addBreakpoint(sourceID, lineNumber);
}

void InspectorController::removeBreakpoint(int sourceID, unsigned lineNumber)
{
    JavaScriptDebugServer::shared().removeBreakpoint(sourceID, lineNumber);
}

static void drawOutlinedRect(GraphicsContext& context, const IntRect& rect, const Color& fillColor)
{
    static const int outlineThickness = 1;
    static const Color outlineColor(62, 86, 180, 228);

    IntRect outline = rect;
    outline.inflate(outlineThickness);

    context.clearRect(outline);

    context.save();
    context.clipOut(rect);
    context.fillRect(outline, outlineColor);
    context.restore();

    context.fillRect(rect, fillColor);
}

static void drawHighlightForBoxes(GraphicsContext& context, const Vector<IntRect>& lineBoxRects, const IntRect& contentBox, const IntRect& paddingBox, const IntRect& borderBox, const IntRect& marginBox)
{
    static const Color contentBoxColor(125, 173, 217, 128);
    static const Color paddingBoxColor(125, 173, 217, 160);
    static const Color borderBoxColor(125, 173, 217, 192);
    static const Color marginBoxColor(125, 173, 217, 228);

    if (!lineBoxRects.isEmpty()) {
        for (size_t i = 0; i < lineBoxRects.size(); ++i)
            drawOutlinedRect(context, lineBoxRects[i], contentBoxColor);
        return;
    }

    if (marginBox != borderBox)
        drawOutlinedRect(context, marginBox, marginBoxColor);
    if (borderBox != paddingBox)
        drawOutlinedRect(context, borderBox, borderBoxColor);
    if (paddingBox != contentBox)
        drawOutlinedRect(context, paddingBox, paddingBoxColor);
    drawOutlinedRect(context, contentBox, contentBoxColor);
}

static inline void convertFromFrameToMainFrame(Frame* frame, IntRect& rect)
{
    rect = frame->page()->mainFrame()->view()->windowToContents(frame->view()->contentsToWindow(rect));
}

void InspectorController::drawNodeHighlight(GraphicsContext& context) const
{
    if (!m_highlightedNode)
        return;

    RenderObject* renderer = m_highlightedNode->renderer();
    Frame* containingFrame = m_highlightedNode->document()->frame();
    if (!renderer || !containingFrame)
        return;

    IntRect contentBox = renderer->absoluteContentBox();
    IntRect boundingBox = renderer->absoluteBoundingBoxRect();

    // FIXME: Should we add methods to RenderObject to obtain these rects?
    IntRect paddingBox(contentBox.x() - renderer->paddingLeft(), contentBox.y() - renderer->paddingTop(), contentBox.width() + renderer->paddingLeft() + renderer->paddingRight(), contentBox.height() + renderer->paddingTop() + renderer->paddingBottom());
    IntRect borderBox(paddingBox.x() - renderer->borderLeft(), paddingBox.y() - renderer->borderTop(), paddingBox.width() + renderer->borderLeft() + renderer->borderRight(), paddingBox.height() + renderer->borderTop() + renderer->borderBottom());
    IntRect marginBox(borderBox.x() - renderer->marginLeft(), borderBox.y() - renderer->marginTop(), borderBox.width() + renderer->marginLeft() + renderer->marginRight(), borderBox.height() + renderer->marginTop() + renderer->marginBottom());

    convertFromFrameToMainFrame(containingFrame, contentBox);
    convertFromFrameToMainFrame(containingFrame, paddingBox);
    convertFromFrameToMainFrame(containingFrame, borderBox);
    convertFromFrameToMainFrame(containingFrame, marginBox);
    convertFromFrameToMainFrame(containingFrame, boundingBox);

    Vector<IntRect> lineBoxRects;
    if (renderer->isInline() || (renderer->isText() && !m_highlightedNode->isSVGElement())) {
        // FIXME: We should show margins/padding/border for inlines.
        renderer->addLineBoxRects(lineBoxRects);
    }

    for (unsigned i = 0; i < lineBoxRects.size(); ++i)
        convertFromFrameToMainFrame(containingFrame, lineBoxRects[i]);

    if (lineBoxRects.isEmpty() && contentBox.isEmpty()) {
        // If we have no line boxes and our content box is empty, we'll just draw our bounding box.
        // This can happen, e.g., with an <a> enclosing an <img style="float:right">.
        // FIXME: Can we make this better/more accurate? The <a> in the above case has no
        // width/height but the highlight makes it appear to be the size of the <img>.
        lineBoxRects.append(boundingBox);
    }

    ASSERT(m_inspectedPage);

    FrameView* view = m_inspectedPage->mainFrame()->view();
    FloatRect overlayRect = view->visibleContentRect();

    if (!overlayRect.contains(boundingBox) && !boundingBox.contains(enclosingIntRect(overlayRect))) {
        Element* element;
        if (m_highlightedNode->isElementNode())
            element = static_cast<Element*>(m_highlightedNode.get());
        else
            element = static_cast<Element*>(m_highlightedNode->parent());
        element->scrollIntoViewIfNeeded();
        overlayRect = view->visibleContentRect();
    }

    context.translate(-overlayRect.x(), -overlayRect.y());

    drawHighlightForBoxes(context, lineBoxRects, contentBox, paddingBox, borderBox, marginBox);
}

bool InspectorController::handleException(JSContextRef context, JSValueRef exception, unsigned lineNumber) const
{
    if (!exception)
        return false;

    if (!m_page)
        return true;

    String message = toString(context, exception, 0);
    String file(__FILE__);

    if (JSObjectRef exceptionObject = JSValueToObject(context, exception, 0)) {
        JSValueRef lineValue = JSObjectGetProperty(context, exceptionObject, jsStringRef("line").get(), NULL);
        if (lineValue)
            lineNumber = static_cast<unsigned>(JSValueToNumber(context, lineValue, 0));

        JSValueRef fileValue = JSObjectGetProperty(context, exceptionObject, jsStringRef("sourceURL").get(), NULL);
        if (fileValue)
            file = toString(context, fileValue, 0);
    }

    m_page->mainFrame()->domWindow()->console()->addMessage(JSMessageSource, ErrorMessageLevel, message, lineNumber, file);
    return true;
}

// JavaScriptDebugListener functions

void InspectorController::didParseSource(ExecState*, const SourceProvider& source, int startingLineNumber, const UString& sourceURL, int sourceID)
{
    JSValueRef sourceIDValue = JSValueMakeNumber(m_scriptContext, sourceID);
    JSValueRef sourceURLValue = JSValueMakeString(m_scriptContext, jsStringRef(sourceURL).get());
    JSValueRef sourceValue = JSValueMakeString(m_scriptContext, jsStringRef(source).get());
    JSValueRef startingLineNumberValue = JSValueMakeNumber(m_scriptContext, startingLineNumber);

    JSValueRef exception = 0;
    JSValueRef arguments[] = { sourceIDValue, sourceURLValue, sourceValue, startingLineNumberValue };
    callFunction(m_scriptContext, m_scriptObject, "parsedScriptSource", 4, arguments, exception);
}

void InspectorController::failedToParseSource(ExecState*, const SourceProvider& source, int startingLineNumber, const UString& sourceURL, int errorLine, const UString& errorMessage)
{
    JSValueRef sourceURLValue = JSValueMakeString(m_scriptContext, jsStringRef(sourceURL).get());
    JSValueRef sourceValue = JSValueMakeString(m_scriptContext, jsStringRef(source.data()).get());
    JSValueRef startingLineNumberValue = JSValueMakeNumber(m_scriptContext, startingLineNumber);
    JSValueRef errorLineValue = JSValueMakeNumber(m_scriptContext, errorLine);
    JSValueRef errorMessageValue = JSValueMakeString(m_scriptContext, jsStringRef(errorMessage).get());

    JSValueRef exception = 0;
    JSValueRef arguments[] = { sourceURLValue, sourceValue, startingLineNumberValue, errorLineValue, errorMessageValue };
    callFunction(m_scriptContext, m_scriptObject, "failedToParseScriptSource", 5, arguments, exception);
}

void InspectorController::didPause()
{
    JSValueRef exception = 0;
    callFunction(m_scriptContext, m_scriptObject, "pausedScript", 0, 0, exception);
}

} // namespace WebCore
