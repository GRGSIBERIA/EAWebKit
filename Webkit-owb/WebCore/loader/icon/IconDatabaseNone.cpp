/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#include "config.h"
#include "IconDatabase.h"
#include "SharedBuffer.h"

namespace WebCore {

static IconDatabase* sharedIconDatabase = 0;

// This version number is in the DB and marks the current generation of the schema
// Theoretically once the switch is flipped this should never change
// Currently, an out-of-date schema causes the DB to be wiped and reset.  This isn't 
// so bad during development but in the future, we would need to write a conversion
// function to advance older released schemas to "current"
const int currentDatabaseVersion = 5;

// Icons expire once a day
const int iconExpirationTime = 60*60*24; 
// Absent icons are rechecked once a week
const int missingIconExpirationTime = 60*60*24*7; 

const int updateTimerDelay = 5; 

String IconDatabase::defaultDatabaseFilename()
{
    static String defaultDatabaseFilename = "Icons.db";
    return defaultDatabaseFilename;
}

IconDatabase* iconDatabase()
{
    if (!sharedIconDatabase)
        sharedIconDatabase = new IconDatabase;
    return sharedIconDatabase;
}

IconDatabase::IconDatabase()
{
}

// 3/26/2009 CSidhall - Memory leak fix for statics
void IconDatabase::staticFinalize()   
{
    if(sharedIconDatabase) {
        delete sharedIconDatabase;
        sharedIconDatabase = 0;
    }
}

bool IconDatabase::open(const String& databasePath)
{
    return false;
}

bool IconDatabase::isOpen() const
{
    return false;
}

void IconDatabase::close()
{
}

String IconDatabase::databasePath() const
{
    return String();
}

void IconDatabase::removeAllIcons()
{
}

void IconDatabase::setPrivateBrowsingEnabled(bool flag)
{
}

bool IconDatabase::isPrivateBrowsingEnabled() const
{
    return false;
}

void IconDatabase::readIconForPageURLFromDisk(const String&)
{

}

Image* IconDatabase::iconForPageURL(const String& pageURL, const IntSize& size)
{
    return defaultIcon(size);
}


IconLoadDecision IconDatabase::loadDecisionForIconURL(const String&, DocumentLoader*)
{
    return IconLoadNo;
}

bool IconDatabase::iconDataKnownForIconURL(const String&)
{
    return false;
}

String IconDatabase::iconURLForPageURL(const String& pageURL)
{
    return String();
}

Image* IconDatabase::defaultIcon(const IntSize& size)
{
    return 0;
}

void IconDatabase::retainIconForPageURL(const String& pageURL)
{
}

void IconDatabase::releaseIconForPageURL(const String& pageURL)
{
}

void IconDatabase::setIconDataForIconURL(PassRefPtr<SharedBuffer> data, const String& iconURL)
{
}

void IconDatabase::setIconURLForPageURL(const String& iconURL, const String& pageURL)
{
}

void IconDatabase::setEnabled(bool enabled)
{
}

bool IconDatabase::isEnabled() const
{
    return false;
}

IconDatabase::~IconDatabase()
{
// 5/5/09 CSidhall - Removed assert as can now be called because the static is now removed with staticFinalize().
//    ASSERT_NOT_REACHED();
}

void IconDatabase::checkIntegrityBeforeOpening()
{
}

void IconDatabase::delayDatabaseCleanup()
{
}

void IconDatabase::allowDatabaseCleanup()
{
}

void IconDatabase::setClient(IconDatabaseClient*)
{
}

} // namespace WebCore
