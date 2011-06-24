#!/usr/bin/perl -w

# Copyright (C) 2005, 2006, 2007 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer. 
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution. 
# 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
#     its contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission. 
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

use strict;

use Config;
use Getopt::Long;
use File::Path;
use IO::File;
use Switch;
use XMLTiny qw(parsefile);

my $tagsFile = "";
my $attrsFile = "";
my $outputDir = ".";
my %tags = ();
my %attrs = ();
my %parameters = ();
my $extraDefines = 0;
my $preprocessor = "/usr/bin/gcc -E -P -x c++";
my %svgCustomMappings = ();
my %htmlCustomMappings = ();

GetOptions('tags=s' => \$tagsFile, 
    'attrs=s' => \$attrsFile,
    'outputDir=s' => \$outputDir,
    'extraDefines=s' => \$extraDefines,
    'preprocessor=s' => \$preprocessor);

die "You must specify at least one of --tags <file> or --attrs <file>" unless (length($tagsFile) || length($attrsFile));

readNames($tagsFile) if length($tagsFile);
readNames($attrsFile) if length($attrsFile);

die "You must specify a namespace (e.g. SVG) for <namespace>Names.h" unless $parameters{'namespace'};
die "You must specify a namespaceURI (e.g. http://www.w3.org/2000/svg)" unless $parameters{'namespaceURI'};
die "You must specify a cppNamespace (e.g. DOM) used for <cppNamespace>::<namespace>Names::fooTag" unless $parameters{'cppNamespace'};

$parameters{'namespacePrefix'} = $parameters{'namespace'} unless $parameters{'namespacePrefix'};

mkpath($outputDir);
my $namesBasePath = "$outputDir/$parameters{'namespace'}Names";
my $factoryBasePath = "$outputDir/$parameters{'namespace'}ElementFactory";
my $wrapperFactoryBasePath = "$outputDir/JS$parameters{'namespace'}ElementWrapperFactory";

printNamesHeaderFile("$namesBasePath.h");
printNamesCppFile("$namesBasePath.cpp");

if ($parameters{'generateFactory'}) {
    printFactoryCppFile("$factoryBasePath.cpp");
    printFactoryHeaderFile("$factoryBasePath.h");
}

if ($parameters{'generateWrapperFactory'}) {
    printWrapperFactoryCppFile("$wrapperFactoryBasePath.cpp");
    printWrapperFactoryHeaderFile("$wrapperFactoryBasePath.h");
}

### Hash initialization

sub initializeTagPropertyHash
{
    return ('upperCase' => upperCaseName($_[0]),
            'applyAudioHack' => 0);
}

sub initializeAttrPropertyHash
{
    return ('upperCase' => upperCaseName($_[0]));
}

sub initializeParametersHash
{
    return ('namespace' => '',
            'namespacePrefix' => '',
            'namespaceURI' => '',
            'cppNamespace' => '',
            'generateFactory' => 0,
            'guardFactoryWith' => '',
            'generateWrapperFactory' => 0,
            # The 2 nullNamespace properties are generated from the "nullNamespace" attribute with respect to the file parsed (attrs or tags).
            'tagsNullNamespace' => 0,
            'attrsNullNamespace' => 0);
}

### Parsing handlers

# Our files should have the following form :
# <'tags' or 'attrs' globalProperty1 = 'value1' ... />
# <'tag/attr name' 'property1' = 'value1' ... />
# where the properties are defined in the initialize*PropertyHash methods.
# (more tag/attr ...)
# </tags> or </attrs>

sub parseTags
{
    my $contentsRef = shift;
    foreach my $contentRef (@$contentsRef) {
        my $tag = $${contentRef}{'name'};
        $tag =~ s/-/_/g;

        # Initialize default properties' values.
        $tags{$tag} = { initializeTagPropertyHash($tag) } if !defined($tags{$tag});

        # Parse the XML attributes.
        my %properties = %{$$contentRef{'attrib'}};
        foreach my $property (keys %properties) {
            die "Unknown property $property for tag $tag\n" if !defined($tags{$tag}{$property});
            $tags{$tag}{$property} = $properties{$property};
        }
    }
}

sub parseAttrs
{
    my $contentsRef = shift;
    foreach my $contentRef (@$contentsRef) {
        my $attr = $${contentRef}{'name'};
        $attr =~ s/-/_/g;

        # Initialize default properties' values.
        $attrs{$attr} = { initializeAttrPropertyHash($attr) } if !defined($attrs{$attr});

        # Parse the XML attributes.
        my %properties = %{$$contentRef{'attrib'}};
        foreach my $property (keys %properties) {
            die "Unknown property $property for attribute $attr\n" if !defined($attrs{$attr}{$property});
            $attrs{$attr}{$property} = $properties{$property};
        }
    }
}

sub parseParameters
{
    my ($propertiesRef, $elementName) = @_;
    my %properties = %$propertiesRef;

    # Initialize default properties' values.
    %parameters = initializeParametersHash() if !(keys %parameters);

    # Parse the XML attributes.
    foreach my $property (keys %properties) {
        # This is used in case we want to change the parameter name depending
        # on what is parsed.
        my $parameter = $property;

        # "nullNamespace" case
        if ($property eq "nullNamespace") {
            $parameter = $elementName.(ucfirst $property);
        }

        die "Unknown parameter $property for tags/attrs\n" if !defined($parameters{$parameter});
        $parameters{$parameter} = $properties{$property};
    }
}

## Support routines

sub readNames
{
    my $namesFile = shift;

    my $names = new IO::File;
    if ($extraDefines eq 0) {
        open($names, $preprocessor . " " . $namesFile . "|") or die "Failed to open file: $namesFile";
    } else {
        open($names, $preprocessor . " -D" . join(" -D", split(" ", $extraDefines)) . " " . $namesFile . "|") or die "Failed to open file: $namesFile";
    }

    # Store hashes keys count to know if some insertion occured.
    my $tagsCount = keys %tags;
    my $attrsCount = keys %attrs;

    my $documentRef = parsefile($names);

    # XML::Tiny returns an array reference to a hash containing the different properties
    my %document = %{@$documentRef[0]};
    my $name = $document{'name'};

    # Check root element to determine what we are parsing
    switch($name) {
        case "tags" {
            parseParameters(\%{$document{'attrib'}}, $name);
            parseTags(\@{$document{'content'}});
        }
        case "attrs" {
            parseParameters(\%{$document{'attrib'}}, $name);
            parseAttrs(\@{$document{'content'}});
        } else {
            die "Do not know how to parse file starting with $name!\n";
        }
    }

    close($names);

    die "Failed to read names from file: $namesFile" if ((keys %tags == $tagsCount) && (keys %attrs == $attrsCount));
}

sub printMacros
{
    my ($F, $macro, $suffix, $namesRef) = @_;
    for my $name (sort keys %$namesRef) {
        print F "    $macro $name","$suffix;\n";
    }
}

sub printConstructors
{
    my ($F, $namesRef) = @_;
    my %names = %$namesRef;

    print F "#if $parameters{'guardFactoryWith'}\n" if $parameters{'guardFactoryWith'};
    for my $name (sort keys %names) {
        my $ucName = $names{$name}{"upperCase"};

        print F "$parameters{'namespace'}Element* ${name}Constructor(Document* doc, bool createdByParser)\n";
        print F "{\n";
        print F "    return new $parameters{'namespace'}${ucName}Element(${name}Tag, doc);\n";
        print F "}\n\n";
    }
    print F "#endif\n" if $parameters{'guardFactoryWith'};
}

sub printFunctionInits
{
    my ($F, $namesRef) = @_;
    for my $name (sort keys %$namesRef) {
        print F "    gFunctionMap->set(${name}Tag.localName().impl(), ${name}Constructor);\n";
    }
}

sub svgCapitalizationHacks
{
    my $name = shift;

    if ($name =~ /^fe(.+)$/) {
        $name = "FE" . ucfirst $1;
    }

    return $name;
}

sub upperCaseName
{
    my $name = shift;
    
    $name = svgCapitalizationHacks($name) if ($parameters{'namespace'} eq "SVG");

    while ($name =~ /^(.*?)_(.*)/) {
        $name = $1 . ucfirst $2;
    }
    
    return ucfirst $name;
}

sub printLicenseHeader
{
    my $F = shift;
    print F "/*
 * THIS FILE IS AUTOMATICALLY GENERATED, DO NOT EDIT.
 *
 *
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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


";
}

sub printNamesHeaderFile
{
    my ($headerPath) = shift;
    my $F;
    open F, ">$headerPath";
    
    printLicenseHeader($F);
    print F "#ifndef DOM_$parameters{'namespace'}NAMES_H\n";
    print F "#define DOM_$parameters{'namespace'}NAMES_H\n\n";
    print F "#include \"QualifiedName.h\"\n\n";
    
    print F "namespace $parameters{'cppNamespace'} { namespace $parameters{'namespace'}Names {\n\n";
    
    my $lowerNamespace = lc($parameters{'namespacePrefix'});
    print F "#ifndef DOM_$parameters{'namespace'}NAMES_HIDE_GLOBALS\n";
    print F "// Namespace\n";
    print F "extern const WebCore::AtomicString ${lowerNamespace}NamespaceURI;\n\n";

    if (keys %tags) {
        print F "// Tags\n";
        printMacros($F, "extern const WebCore::QualifiedName", "Tag", \%tags);
        print F "\n\nWebCore::QualifiedName** get$parameters{'namespace'}Tags(size_t* size);\n";
    }
    
    if (keys %attrs) {
        print F "// Attributes\n";
        printMacros($F, "extern const WebCore::QualifiedName", "Attr", \%attrs);
        print F "\n\nWebCore::QualifiedName** get$parameters{'namespace'}Attr(size_t* size);\n";
    }
    print F "#endif\n\n";
    print F "void init();\n\n";
    print F "} }\n\n";
    print F "#endif\n\n";
    
    close F;
}

sub printNamesCppFile
{
    my $cppPath = shift;
    my $F;
    open F, ">$cppPath";
    
    printLicenseHeader($F);
    
    my $lowerNamespace = lc($parameters{'namespacePrefix'});

print F "#include \"config.h\"\n";

print F "#ifdef AVOID_STATIC_CONSTRUCTORS\n";
print F "#define DOM_$parameters{'namespace'}NAMES_HIDE_GLOBALS 1\n";
print F "#else\n";
print F "#define QNAME_DEFAULT_CONSTRUCTOR 1\n";
print F "#endif\n\n";


print F "#include \"$parameters{'namespace'}Names.h\"\n\n";
print F "#include \"StaticConstructors.h\"\n";

print F "namespace $parameters{'cppNamespace'} { namespace $parameters{'namespace'}Names {

using namespace WebCore;

DEFINE_GLOBAL(AtomicString, ${lowerNamespace}NamespaceURI, \"$parameters{'namespaceURI'}\")
";

    if (keys %tags) {
        print F "// Tags\n";
        for my $name (sort keys %tags) {
            print F "DEFINE_GLOBAL(QualifiedName, ", $name, "Tag, nullAtom, \"$name\", ${lowerNamespace}NamespaceURI);\n";
        }
        
        print F "\n\nWebCore::QualifiedName** get$parameters{'namespace'}Tags(size_t* size)\n";
        print F "{\n    static WebCore::QualifiedName* $parameters{'namespace'}Tags[] = {\n";
        for my $name (sort keys %tags) {
            print F "        (WebCore::QualifiedName*)&${name}Tag,\n";
        }
        print F "    };\n";
        print F "    *size = ", scalar(keys %tags), ";\n";
        print F "    return $parameters{'namespace'}Tags;\n";
        print F "}\n";
        
    }

    if (keys %attrs) {
        print F "\n// Attributes\n";
        for my $name (sort keys %attrs) {
            print F "DEFINE_GLOBAL(QualifiedName, ", $name, "Attr, nullAtom, \"$name\", ${lowerNamespace}NamespaceURI);\n";
        }
        print F "\n\nWebCore::QualifiedName** get$parameters{'namespace'}Attrs(size_t* size)\n";
        print F "{\n    static WebCore::QualifiedName* $parameters{'namespace'}Attr[] = {\n";
        for my $name (sort keys %attrs) {
            print F "        (WebCore::QualifiedName*)&${name}Attr,\n";
        }
        print F "    };\n";
        print F "    *size = ", scalar(keys %attrs), ";\n";
        print F "    return $parameters{'namespace'}Attr;\n";
        print F "}\n";
    }

print F "\nvoid init()
{
    static bool initialized = false;
    if (initialized)
        return;
    initialized = true;
    
    // Use placement new to initialize the globals.
    
    AtomicString::init();
";
    
    print(F "    AtomicString ${lowerNamespace}NS(\"$parameters{'namespaceURI'}\");\n\n");

    print(F "    // Namespace\n");
    print(F "    new ((void*)&${lowerNamespace}NamespaceURI) AtomicString(${lowerNamespace}NS);\n\n");
    if (keys %tags) {
        my $tagsNamespace = $parameters{'tagsNullNamespace'} ? "nullAtom" : "${lowerNamespace}NS";
        printDefinitions($F, \%tags, "tags", $tagsNamespace);
    }
    if (keys %attrs) {
        my $attrsNamespace = $parameters{'attrsNullNamespace'} ? "nullAtom" : "${lowerNamespace}NS";
        printDefinitions($F, \%attrs, "attributes", $attrsNamespace);
    }

    print F "}\n\n} }\n\n";
    close F;
}

sub printJSElementIncludes
{
    my ($F, $namesRef) = @_;
    my %names = %$namesRef;
    for my $name (sort keys %names) {
        next if (hasCustomMapping($name));

        my $ucName = $names{$name}{"upperCase"};
        print F "#include \"JS$parameters{'namespace'}${ucName}Element.h\"\n";
    }
}

sub printElementIncludes
{
    my ($F, $namesRef, $shouldSkipCustomMappings) = @_;
    my %names = %$namesRef;
    for my $name (sort keys %names) {
        next if ($shouldSkipCustomMappings && hasCustomMapping($name));

        my $ucName = $names{$name}{"upperCase"};
        print F "#include \"$parameters{'namespace'}${ucName}Element.h\"\n";
    }
}

sub printDefinitions
{
    my ($F, $namesRef, $type, $namespaceURI) = @_;
    my $singularType = substr($type, 0, -1);
    my $shortType = substr($singularType, 0, 4);
    my $shortCamelType = ucfirst($shortType);
    my $shortUpperType = uc($shortType);
    
    print F "    // " . ucfirst($type) . "\n";

    for my $name (sort keys %$namesRef) {
        print F "    const char *$name","${shortCamelType}String = \"$name\";\n";
    }
        
    for my $name (sort keys %$namesRef) {
        if ($name =~ /_/) {
            my $realName = $name;
            $realName =~ s/_/-/g;
            print F "    ${name}${shortCamelType}String = \"$realName\";\n";
        }
    }
    print F "\n";

    for my $name (sort keys %$namesRef) {
        print F "    new ((void*)&$name","${shortCamelType}) QualifiedName(nullAtom, $name","${shortCamelType}String, $namespaceURI);\n";
    }

}

## ElementFactory routines

sub printFactoryCppFile
{
    my $cppPath = shift;
    my $F;
    open F, ">$cppPath";

printLicenseHeader($F);

print F <<END
#include "config.h"
#include "$parameters{'namespace'}ElementFactory.h"
#include "$parameters{'namespace'}Names.h"
#include "Page.h"
#include "Settings.h"
END
;

printElementIncludes($F, \%tags, 0);

print F <<END
#include <wtf/HashMap.h>

using namespace WebCore;
using namespace $parameters{'cppNamespace'}::$parameters{'namespace'}Names;

typedef $parameters{'namespace'}Element* (*ConstructorFunc)(Document* doc, bool createdByParser);
typedef WTF::HashMap<AtomicStringImpl*, ConstructorFunc> FunctionMap;

static FunctionMap* gFunctionMap = 0;

namespace $parameters{'cppNamespace'} {

END
;

printConstructors($F, \%tags);

print F "#if $parameters{'guardFactoryWith'}\n" if $parameters{'guardFactoryWith'};

print F <<END
static inline void createFunctionMapIfNecessary()
{
    if (gFunctionMap)
        return;
    // Create the table.
    gFunctionMap = new FunctionMap;
    
    // Populate it with constructor functions.
END
;

printFunctionInits($F, \%tags);

print F "}\n";
print F "#endif\n\n" if $parameters{'guardFactoryWith'};

print F <<END
$parameters{'namespace'}Element* $parameters{'namespace'}ElementFactory::create$parameters{'namespace'}Element(const QualifiedName& qName, Document* doc, bool createdByParser)
{
END
;

print F "#if $parameters{'guardFactoryWith'}\n" if $parameters{'guardFactoryWith'};

print F <<END
    // Don't make elements without a document
    if (!doc)
        return 0;

#if ENABLE(DASHBOARD_SUPPORT)
    Settings* settings = doc->settings();
    if (settings && settings->usesDashboardBackwardCompatibilityMode())
        return 0;
#endif

    createFunctionMapIfNecessary();
    ConstructorFunc func = gFunctionMap->get(qName.localName().impl());
    if (func)
        return func(doc, createdByParser);

    return new $parameters{'namespace'}Element(qName, doc);
END
;

if ($parameters{'guardFactoryWith'}) {

print F <<END
#else
    return 0;
#endif
END
;

}

print F <<END
}

} // namespace

END
;

    close F;
}

sub printFactoryHeaderFile
{
    my $headerPath = shift;
    my $F;
    open F, ">$headerPath";

    printLicenseHeader($F);

print F "#ifndef $parameters{'namespace'}ELEMENTFACTORY_H\n";
print F "#define $parameters{'namespace'}ELEMENTFACTORY_H\n\n";

print F "
namespace WebCore {
    class Element;
    class Document;
    class QualifiedName;
    class AtomicString;
}

namespace $parameters{'cppNamespace'}
{
    class $parameters{'namespace'}Element;

    // The idea behind this class is that there will eventually be a mapping from namespace URIs to ElementFactories that can dispense
    // elements.  In a compound document world, the generic createElement function (will end up being virtual) will be called.
    class $parameters{'namespace'}ElementFactory
    {
    public:
        WebCore::Element* createElement(const WebCore::QualifiedName& qName, WebCore::Document* doc, bool createdByParser = true);
        static $parameters{'namespace'}Element* create$parameters{'namespace'}Element(const WebCore::QualifiedName& qName, WebCore::Document* doc, bool createdByParser = true);
    };
}

#endif

";

    close F;
}

## Wrapper Factory routines

sub initializeCustomMappings
{
    if (!keys %svgCustomMappings) {
        # These are used to map a tag to another one in WrapperFactory
        # (for example, "h2" is mapped to "h1" so that they use the same JS Wrapper ("h1" wrapper))
        # Mapping to an empty string will not generate a wrapper
        %svgCustomMappings = ('animateMotion' => '',
                              'hkern' => '',
                              'mpath' => '');
        %htmlCustomMappings = ('abbr' => '',
                               'acronym' => '',
                               'address' => '',
                               'b' => '',
                               'bdo' => '',
                               'big' => '',
                               'center' => '',
                               'cite' => '',
                               'code' => '',
                               'colgroup' => 'col',
                               'dd' => '',
                               'dfn' => '',
                               'dt' => '',
                               'em' => '',
                               'h2' => 'h1',
                               'h3' => 'h1',
                               'h4' => 'h1',
                               'h5' => 'h1',
                               'h6' => 'h1',
                               'i' => '',
                               'image' => 'img',
                               'ins' => 'del',
                               'kbd' => '',
                               'keygen' => 'select',
                               'listing' => 'pre',
                               'layer' => '',
                               'nobr' => '',
                               'noembed' => '',
                               'noframes' => '',
                               'nolayer' => '',
                               'noscript' => '',
                               'plaintext' => '',
                               's' => '',
                               'samp' => '',
                               'small' => '',
                               'span' => '',
                               'strike' => '',
                               'strong' => '',
                               'sub' => '',
                               'sup' => '',
                               'tfoot' => 'tbody',
                               'th' => 'td',
                               'thead' => 'tbody',
                               'tt' => '',
                               'u' => '',
                               'var' => '',
                               'wbr' => '',
                               'xmp' => 'pre');
    }
}

sub hasCustomMapping
{
    my $name = shift;
    initializeCustomMappings();
    return 1 if $parameters{'namespace'} eq "HTML" && exists($htmlCustomMappings{$name});
    return 1 if $parameters{'namespace'} eq "SVG" && exists($svgCustomMappings{$name});
    return 0;
}

sub printWrapperFunctions
{
    my ($F, $namesRef) = @_;
    my %names = %$namesRef;
    for my $name (sort keys %names) {
        # Custom mapping do not need a JS wrapper
        next if (hasCustomMapping($name));

        my $ucName = $names{$name}{"upperCase"};
        # Hack for the media tags
        if ($names{$name}{"applyAudioHack"}) {
            print F <<END
static JSNode* create${ucName}Wrapper(ExecState* exec, PassRefPtr<$parameters{'namespace'}Element> element)
{
    if (!MediaPlayer::isAvailable())
        return new (exec) JS$parameters{'namespace'}Element(JS$parameters{'namespace'}ElementPrototype::self(exec), element.get());
    return new (exec) JS$parameters{'namespace'}${ucName}Element(JS$parameters{'namespace'}${ucName}ElementPrototype::self(exec), static_cast<$parameters{'namespace'}${ucName}Element*>(element.get()));
}

END
;
        } else {
            print F <<END
static JSNode* create${ucName}Wrapper(ExecState* exec, PassRefPtr<$parameters{'namespace'}Element> element)
{   
    return new (exec) JS$parameters{'namespace'}${ucName}Element(JS$parameters{'namespace'}${ucName}ElementPrototype::self(exec), static_cast<$parameters{'namespace'}${ucName}Element*>(element.get()));
}

END
;
        }
    }
}

sub printWrapperFactoryCppFile
{
    my $cppPath = shift;
    my $F;
    open F, ">$cppPath";

    printLicenseHeader($F);

    print F "#include \"config.h\"\n\n";

    print F "#if $parameters{'guardFactoryWith'}\n\n" if $parameters{'guardFactoryWith'};

    print F "#include \"JS$parameters{'namespace'}ElementWrapperFactory.h\"\n";

    printJSElementIncludes($F, \%tags);

    print F "\n#include \"$parameters{'namespace'}Names.h\"\n\n";

    printElementIncludes($F, \%tags, 1);

    print F <<END
using namespace KJS;

namespace WebCore {

using namespace $parameters{'namespace'}Names;

typedef JSNode* (*Create$parameters{'namespace'}ElementWrapperFunction)(ExecState*, PassRefPtr<$parameters{'namespace'}Element>);

END
;

    printWrapperFunctions($F, \%tags);

    print F <<END
JSNode* createJS$parameters{'namespace'}Wrapper(ExecState* exec, PassRefPtr<$parameters{'namespace'}Element> element)
{   
    static HashMap<WebCore::AtomicStringImpl*, Create$parameters{'namespace'}ElementWrapperFunction> map;
    if (map.isEmpty()) {
END
;

    for my $tag (sort keys %tags) {
        next if (hasCustomMapping($tag));

        my $ucTag = $tags{$tag}{"upperCase"};
        print F "       map.set(${tag}Tag.localName().impl(), create${ucTag}Wrapper);\n";
    }

    if ($parameters{'namespace'} eq "HTML") {
        for my $tag (sort keys %htmlCustomMappings) {
            next if !$htmlCustomMappings{$tag};

            my $ucCustomTag = $tags{$htmlCustomMappings{$tag}}{"upperCase"};
            print F "       map.set(${tag}Tag.localName().impl(), create${ucCustomTag}Wrapper);\n";
        }
    }

    # Currently SVG has no need to add custom map.set as it only has empty elements

    print F <<END
    }
    Create$parameters{'namespace'}ElementWrapperFunction createWrapperFunction = map.get(element->localName().impl());
    if (createWrapperFunction)
        return createWrapperFunction(exec, element);
    return new (exec) JS$parameters{'namespace'}Element(JS$parameters{'namespace'}ElementPrototype::self(exec), element.get());
}

}

END
;

    print F "#endif\n" if $parameters{'guardFactoryWith'};

    close F;
}

sub printWrapperFactoryHeaderFile
{
    my $headerPath = shift;
    my $F;
    open F, ">$headerPath";

    printLicenseHeader($F);

    print F "#ifndef JS$parameters{'namespace'}ElementWrapperFactory_h\n";
    print F "#define JS$parameters{'namespace'}ElementWrapperFactory_h\n\n";

    print F "#if $parameters{'guardFactoryWith'}\n" if $parameters{'guardFactoryWith'};

    print F <<END
#include <wtf/Forward.h>

namespace KJS {
    class ExecState;
}                                            
                                             
namespace WebCore {

    class JSNode;
    class $parameters{'namespace'}Element;

    JSNode* createJS$parameters{'namespace'}Wrapper(KJS::ExecState*, PassRefPtr<$parameters{'namespace'}Element>);

}
 
END
;

    print F "#endif // $parameters{'guardFactoryWith'}\n\n" if $parameters{'guardFactoryWith'};

    print F "#endif // JS$parameters{'namespace'}ElementWrapperFactory_h\n";

    close F;
}
