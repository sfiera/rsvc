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
#include <sys/param.h>
CF_EXPORT void CFLog(int32_t level, CFStringRef format, ...);

#include "common.h"

const char* rsvc_disc_type_name[] = {
    [RSVC_DISC_TYPE_NONE]   = "none",
    [RSVC_DISC_TYPE_CD]     = "cd",
    [RSVC_DISC_TYPE_DVD]    = "dvd",
    [RSVC_DISC_TYPE_BD]     = "bd",
    [RSVC_DISC_TYPE_HDDVD]  = "hddvd",
};

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

struct watch_context {
    dispatch_queue_t queue;
    bool enable;
    struct watch_node* head;
    rsvc_disc_watch_callbacks_t callbacks;
};

struct watch_node {
    char* name;
    rsvc_disc_type_t type;
    struct watch_node* next;
};

static void yield_disc(struct watch_context* watch,
                       io_object_t object) {
    io_name_t type_string;
    if (IOObjectGetClass(object, type_string) != KERN_SUCCESS) {
        return;
    }
    rsvc_disc_type_t type;
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
            for (struct watch_node* node = watch->head; node; node = node->next) {
                if (strcmp(node->name, name) == 0) {
                    goto yield_cleanup;
                }
            }
            struct watch_node head = {
                .name = strdup(name),
                .type = type,
                .next = watch->head,
            };
            watch->head = memdup(&head, sizeof(head));
            watch->callbacks.appeared(type, name);
        }
    }
yield_cleanup:
    CFRelease(cfpath);
}

static void start_watch(struct watch_context* watch, rsvc_stop_t stop) {
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
        yield_disc(watch, object);
        IOObjectRelease(object);
    }

    watch->callbacks.initialized(stop);
    watch->enable = true;
}

static void appeared_callback(DADiskRef disk, void* userdata) {
    struct watch_context* watch = userdata;
    if (watch->enable) {
        io_service_t object = DADiskCopyIOMedia(disk);
        yield_disc(watch, object);
        IOObjectRelease(object);
    }
}

static void disappeared_callback(DADiskRef disk, void* userdata) {
    struct watch_context* watch = userdata;
    if (watch->enable) {
        const char* name = DADiskGetBSDName(disk);
        struct watch_node** curr = &watch->head;
        while (*curr) {
            struct watch_node* node = *curr;
            if (strcmp(name, node->name) == 0) {
                io_service_t object = DADiskCopyIOMedia(disk);
                watch->callbacks.disappeared(node->type, node->name);
                IOObjectRelease(object);

                *curr = node->next;
                free(node->name);
                free(node);
                return;
            }
            curr = &node->next;
        }
    }
}

void rsvc_disc_watch(rsvc_disc_watch_callbacks_t callbacks) {
    struct watch_context build_userdata = {
        .queue = dispatch_queue_create("net.sfiera.ripservice.disc", NULL),
        .enable = false,
        .head = NULL,
        .callbacks = {
            .appeared = Block_copy(callbacks.appeared),
            .disappeared = Block_copy(callbacks.disappeared),
            .initialized = Block_copy(callbacks.initialized),
        },
    };
    struct watch_context* userdata = memdup(&build_userdata, sizeof(build_userdata));

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
            while (userdata->head) {
                struct watch_node* node = userdata->head;
                userdata->head = node->next;
                free(node->name);
                free(node);
            }
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

static void da_callback(DADiskRef disk, DADissenterRef dissenter, void *userdata) {
    rsvc_done_t done = userdata;
    if (dissenter) {
        CFStringRef cfstr = DADissenterGetStatusString(dissenter);
        int status = DADissenterGetStatus(dissenter);
        if (cfstr) {
            size_t size = CFStringGetLength(cfstr) + 1;
            char* str = malloc(size);
            if (CFStringGetCString(cfstr, str, size, kCFStringEncodingUTF8)) {
                rsvc_errorf(done, __FILE__, __LINE__, "%s", str);
            } else {
                rsvc_errorf(done, __FILE__, __LINE__, "CFStringGetCString");
            }
            free(str);
        }
        rsvc_errorf(done, __FILE__, __LINE__, "DADissenterGetStatusString: %x", status);
    } else {
        done(NULL);
    }
    Block_release(done);
}

void rsvc_disc_eject(const char* path, rsvc_done_t done) {
    dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    DASessionRef session = DASessionCreate(NULL);
    DASessionSetDispatchQueue(session, queue);
    DADiskRef disk = DADiskCreateFromBSDName(NULL, session, path);

    done = ^(rsvc_error_t error){
        if (error) {
            rsvc_errorf(done, __FILE__, __LINE__,
                        "%s: %s", DADiskGetBSDName(disk), error->message);
        } else {
            done(NULL);
        }
        CFRelease(disk);
        CFRelease(session);
    };

    dispatch_async(queue, ^{
        CFDictionaryRef dict = DADiskCopyDescription(disk);
        if (!dict) {
            rsvc_errorf(done, __FILE__, __LINE__, "no such disk");
            return;
        }
        bool mounted = CFDictionaryGetValueIfPresent(dict, kDADiskDescriptionVolumePathKey,
                                                     NULL);
        bool ejectable = CFDictionaryGetValue(dict, kDADiskDescriptionMediaEjectableKey)
                         == kCFBooleanTrue;
        CFRelease(dict);

        if (!ejectable) {
            rsvc_errorf(done, __FILE__, __LINE__, "not ejectable");
        } else if (mounted) {
            rsvc_done_t eject = ^(rsvc_error_t error){
                if (error) {
                    done(error);
                    return;
                }
                DADiskEject(disk, kDADiskEjectOptionDefault, da_callback, Block_copy(done));
            };
            DADiskUnmount(disk, kDADiskUnmountOptionDefault, da_callback, Block_copy(eject));
        } else {
            DADiskEject(disk, kDADiskEjectOptionDefault, da_callback, Block_copy(done));
        }
    });
}
