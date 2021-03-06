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

#include <Block.h>
#include <CoreFoundation/CoreFoundation.h>
#include <DiskArbitration/DiskArbitration.h>
#include <IOKit/IOBSD.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOBDMedia.h>
#include <IOKit/storage/IOCDMedia.h>
#include <IOKit/storage/IODVDMedia.h>
#include <IOKit/storage/IOMedia.h>
#include <mach/error.h>
#include <sys/param.h>

#include "disc.h"
#include "list.h"
#include "common.h"

static bool get_type(const char* name, enum rsvc_disc_type* type) {
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

static void send_io_object(struct rsvc_watch_context* watch, io_object_t object) {
    io_name_t type_string;
    if (IOObjectGetClass(object, type_string) != KERN_SUCCESS) {
        return;
    }
    enum rsvc_disc_type type;
    if (!get_type(type_string, &type)) {
        return;
    }

    // Other interesting properties:
    //   Ejectable
    //   IOMediaIcon
    //   Type
    //   TOC
    CFTypeRef cfpath = IORegistryEntryCreateCFProperty(
            object, CFSTR(kIOBSDNameKey), kCFAllocatorDefault, 0);
    if (cfpath) {
        char name[MAXPATHLEN] = {};
        if (CFStringGetCString(cfpath, name, MAXPATHLEN, kCFStringEncodingUTF8)) {
            rsvc_send_disc(watch, name, type);
        }
    }
    CFRelease(cfpath);
}

static void start_watch(struct rsvc_watch_context* watch, rsvc_stop_t stop) {
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

    io_object_t object;
    while ((object = IOIteratorNext(cds))) {
        send_io_object(watch, object);
        IOObjectRelease(object);
    }

    watch->callbacks.initialized(stop);
    watch->enable = true;
}

static void appeared_callback(DADiskRef disk, void* userdata) {
    struct rsvc_watch_context* watch = userdata;
    if (watch->enable) {
        io_service_t object = DADiskCopyIOMedia(disk);
        send_io_object(watch, object);
        IOObjectRelease(object);
    }
}

static void disappeared_callback(DADiskRef disk, void* userdata) {
    struct rsvc_watch_context* watch = userdata;
    if (watch->enable) {
        const char* name = DADiskGetBSDName(disk);
        rsvc_send_disc(watch, name, RSVC_DISC_TYPE_NONE);
    }
}

void rsvc_disc_watch(struct rsvc_disc_watch_callbacks callbacks) {
    struct rsvc_watch_context build_userdata = {
        .queue = dispatch_queue_create("net.sfiera.ripservice.disc", NULL),
        .enable = false,
        .callbacks = {
            .appeared = Block_copy(callbacks.appeared),
            .disappeared = Block_copy(callbacks.disappeared),
            .initialized = Block_copy(callbacks.initialized),
        },
    };
    struct rsvc_watch_context* userdata = memdup(&build_userdata, sizeof(build_userdata));

    DASessionRef session = DASessionCreate(kCFAllocatorDefault);
    DASessionSetDispatchQueue(session, userdata->queue);

    __block rsvc_stop_t stop = ^{
        // First, unregister the callbacks.
        DAUnregisterCallback(session, disappeared_callback, userdata);
        DAUnregisterCallback(session, appeared_callback, userdata);

        // But, there might be pending callbacks still in the queue, or
        // we might still be awaiting initialization.  Schedule the
        // deletion for after they finish.
        dispatch_async(userdata->queue, ^{
            RSVC_LIST_CLEAR(userdata, ^(struct disc_node* node){
                free(node->name);
            });
            Block_release(userdata->callbacks.disappeared);
            Block_release(userdata->callbacks.appeared);
            Block_release(userdata->callbacks.initialized);
            free(userdata);
            Block_release(stop);
            CFRelease(session);
        });
    };
    stop = Block_copy(stop);
    DARegisterDiskAppearedCallback(session, NULL, appeared_callback, userdata);
    DARegisterDiskDisappearedCallback(session, NULL, disappeared_callback, userdata);
    dispatch_async(userdata->queue, ^{
        start_watch(userdata, stop);
    });
}

struct da_data {
    bool                  ok;
    dispatch_semaphore_t  sema;
    rsvc_done_t           fail;
};

static void dissent_error(rsvc_done_t fail, const char* file, int lineno, uint32_t status) {
    if ((status & system_emask) == err_kern) {
        if ((status & sub_emask) == err_sub(3)) {
            errno = err_get_code(status);
            rsvc_strerrorf(fail, file, lineno, NULL);
        }
    }
    rsvc_errorf(fail, file, lineno, "dissent error: $%08x", status);
}

static void da_callback(DADiskRef disk, DADissenterRef dissenter, void *userdata) {
    (void)disk;
    struct da_data* da = userdata;
    if (dissenter) {
        int status = DADissenterGetStatus(dissenter);
        CFStringRef cfstr = DADissenterGetStatusString(dissenter);
        if (cfstr) {
            size_t size = CFStringGetLength(cfstr) + 1;
            char* str = malloc(size);
            if (CFStringGetCString(cfstr, str, size, kCFStringEncodingUTF8)) {
                rsvc_errorf(da->fail, __FILE__, __LINE__, "%s", str);
            } else {
                dissent_error(da->fail, __FILE__, __LINE__, status);
            }
            free(str);
        } else {
            dissent_error(da->fail, __FILE__, __LINE__, status);
        }
        da->ok = false;
    } else {
        da->ok = true;
    }
    dispatch_semaphore_signal(da->sema);
}

static bool copy_disk_description(DADiskRef disk, CFDictionaryRef* dict, rsvc_done_t fail) {
    *dict = DADiskCopyDescription(disk);
    if (!*dict) {
        rsvc_errorf(fail, __FILE__, __LINE__, "no such disk");
        return false;
    }
    return true;
}

bool rsvc_disc_eject(const char* path, rsvc_done_t fail) {
    dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    DASessionRef session = DASessionCreate(NULL);
    DASessionSetDispatchQueue(session, queue);
    DADiskRef disk = DADiskCreateFromBSDName(NULL, session, path);

    fail = ^(rsvc_error_t error){
        rsvc_errorf(fail, error->file, error->lineno,
                    "%s: %s", DADiskGetBSDName(disk), error->message);
    };

    struct da_data da = {
        .ok = false,
        .fail = fail,
    };
    CFDictionaryRef dict;
    if (copy_disk_description(disk, &dict, fail)) {
        bool mounted = CFDictionaryGetValueIfPresent(dict, kDADiskDescriptionVolumePathKey,
                                                     NULL);
        bool ejectable = CFDictionaryGetValue(dict, kDADiskDescriptionMediaEjectableKey)
                         == kCFBooleanTrue;
        CFRelease(dict);

        da.sema = dispatch_semaphore_create(0);
        if (!ejectable) {
            rsvc_errorf(fail, __FILE__, __LINE__, "not ejectable");
        } else if (mounted) {
            DADiskUnmount(disk, kDADiskUnmountOptionDefault, da_callback, &da);
            dispatch_semaphore_wait(da.sema, DISPATCH_TIME_FOREVER);
            if (da.ok) {
                DADiskEject(disk, kDADiskEjectOptionDefault, da_callback, &da);
                dispatch_semaphore_wait(da.sema, DISPATCH_TIME_FOREVER);
            }
        } else {
            DADiskEject(disk, kDADiskEjectOptionDefault, da_callback, &da);
            dispatch_semaphore_wait(da.sema, DISPATCH_TIME_FOREVER);
        }
        dispatch_release(da.sema);
    }
    CFRelease(session);
    return da.ok;
}
