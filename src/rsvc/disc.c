//
// This file is part of Rip Service.
//
// Copyright (C) 2012 Chris Pickel <sfiera@sfzmail.com>
//
// Rip Service is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or (at
// your option) any later version.
//
// Rip Service is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Rip Service; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#include <rsvc/disc.h>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOBSD.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOBDMedia.h>
#include <IOKit/storage/IOCDMedia.h>
#include <IOKit/storage/IODVDMedia.h>
#include <IOKit/storage/IOMedia.h>
#include <sys/param.h>

static bool get_type(const char* name, rsvc_disc_type_t* type) {
    if (strcmp(name, kIOCDMediaClass) == 0) {
        *type = RSVC_DISC_TYPE_CD;
        return true;
    } else if (strcmp(name, kIODVDMediaClass) == 0) {
        *type = RSVC_DISC_TYPE_DVD;
        return true;
    } else if (strcmp(name, kIOBDMediaClass) == 0) {
        *type = RSVC_DISC_TYPE_BD;
        return true;
    }
    return false;
}

void rsvc_disc_ls(void (^block)(rsvc_disc_type_t type, const char* path)) {
    CFMutableDictionaryRef classes = IOServiceMatching(kIOMediaClass);
    if (classes == NULL) {
        return;
    } else {
        CFDictionarySetValue(classes, CFSTR(kIOMediaEjectableKey), kCFBooleanTrue);
    }

    io_iterator_t cds;
    if (IOServiceGetMatchingServices(kIOMasterPortDefault, classes, &cds) != KERN_SUCCESS) {
        return;
    }

    char path[MAXPATHLEN] = {};

    io_object_t object;
    while ((object = IOIteratorNext(cds))) {
        io_name_t name;
        if (IOObjectGetClass(object, name) != KERN_SUCCESS) {
            continue;
        }
        rsvc_disc_type_t type;
        if (!get_type(name, &type)) {
            continue;
        }

        // Other interesting properties:
        //   Ejectable
        //   IOMediaIcon
        //   Type
        //   TOC
        CFTypeRef cfpath = IORegistryEntryCreateCFProperty(
                object, CFSTR(kIOBSDNameKey), kCFAllocatorDefault, 0);
        if (cfpath) {
            if (CFStringGetCString(cfpath, path, MAXPATHLEN, kCFStringEncodingUTF8)) {
                block(type, path);
            }
            CFRelease(cfpath);
        }
        IOObjectRelease(object);
    }
}
