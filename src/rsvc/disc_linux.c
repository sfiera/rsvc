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

#define _POSIX_C_SOURCE 200809L

#include <rsvc/disc.h>

#include <Block.h>
#include <fcntl.h>
#include <libudev.h>
#include <linux/cdrom.h>
#include <locale.h>
#include <rsvc/common.h>
#include <scsi/sg.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "disc.h"
#include "unix.h"

struct rsvc_watch_context context;

static const char* abbrev_dev(const char* devnode) {
    const char pfx[] = "/dev/";
    if ((memcmp(pfx, devnode, strlen(pfx)) == 0)
        && (strchr(devnode + strlen(pfx), '/') == NULL)) {
        return devnode + strlen(pfx);
    }
    return devnode;
}

static enum rsvc_disc_type profile_disc_type(int profile) {
    switch (profile) {
      case 0x08:
      case 0x09:
      case 0x0a:
        return RSVC_DISC_TYPE_CD;
      case 0x10:
      case 0x11:
      case 0x12:
      case 0x13:
      case 0x14:
      case 0x15:
      case 0x16:
      case 0x17:
      case 0x18:
      case 0x1a:
      case 0x1b:
      case 0x2a:
      case 0x2b:
        return RSVC_DISC_TYPE_DVD;
      case 0x40:
      case 0x41:
      case 0x42:
      case 0x43:
        return RSVC_DISC_TYPE_BD;
      case 0x50:
      case 0x51:
      case 0x52:
      case 0x53:
      case 0x58:
      case 0x5a:
        return RSVC_DISC_TYPE_HDDVD;
      default:
        return RSVC_DISC_TYPE_NONE;
    }
}

static enum rsvc_disc_type fd_to_disc_type(int fd) {
    unsigned char buf[8] = {};
    unsigned char command[10] = {
        [0] = 0x46,
        [8] = sizeof(buf),
    };
    struct request_sense s = {};

    struct sg_io_hdr sg_io = {
        .interface_id = 'S',
        .sbp = (unsigned char*)&s,
        .mx_sb_len = sizeof(s),
        .cmdp = command,
        .cmd_len = sizeof(command),
        .flags = SG_FLAG_LUN_INHIBIT | SG_FLAG_DIRECT_IO,
        .dxferp = buf,
        .dxfer_len = sizeof(buf),
        .dxfer_direction = SG_DXFER_FROM_DEV,
    };

    if (ioctl(fd, SG_IO, &sg_io)) {
        return RSVC_DISC_TYPE_NONE;
    }
    int profile = (buf[6] << 8) | buf[7];
    return profile_disc_type(profile);
}

static enum rsvc_disc_type path_to_disc_type(const char* path) {
    int fd;
    rsvc_done_t fail = ^(rsvc_error_t error){ (void)error; };
    if (!rsvc_opendev(path, O_RDONLY | O_NONBLOCK, 0, &fd, fail)) {
        return RSVC_DISC_TYPE_NONE;
    }
    enum rsvc_disc_type type = fd_to_disc_type(fd);
    close(fd);
    return type;
}

static void disc_watch(struct rsvc_watch_context* context) {
	struct udev *udev = udev_new();
	if (!udev) {
        return;
	}

   	struct udev_monitor* monitor = udev_monitor_new_from_netlink(udev, "udev");
	udev_monitor_filter_add_match_subsystem_devtype(monitor, "block", NULL);
	udev_monitor_enable_receiving(monitor);
	int udev_fd = udev_monitor_get_fd(monitor);
    int event_fd = eventfd(0, EFD_NONBLOCK);

	struct udev_enumerate *enumerate = udev_enumerate_new(udev);
	udev_enumerate_add_match_subsystem(enumerate, "block");
	udev_enumerate_scan_devices(enumerate);
    struct udev_list_entry *blocks = udev_enumerate_get_list_entry(enumerate);
	struct udev_list_entry *dev_list_entry;
	udev_list_entry_foreach(dev_list_entry, blocks) {
		const char *path = udev_list_entry_get_name(dev_list_entry);
		struct udev_device* dev = udev_device_new_from_syspath(udev, path);
        const char* devnode = udev_device_get_devnode(dev);
        enum rsvc_disc_type type = path_to_disc_type(devnode);
        rsvc_send_disc(context, abbrev_dev(devnode), type);
		udev_device_unref(dev);
	}
	udev_enumerate_unref(enumerate);

    context->callbacks.initialized(^{
        if (event_fd >= 0) {
            uint64_t i = 1;
            write(event_fd, &i, sizeof(i));
        }
    });

	while (1) {
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(udev_fd, &fds);
		FD_SET(event_fd, &fds);
        int max_fd = (udev_fd > event_fd) ? udev_fd : event_fd;

		int ret = select(max_fd + 1, &fds, NULL, NULL, NULL);
		if (ret > 0) {
            if (FD_ISSET(event_fd, &fds)) {
                uint64_t i;
                read(event_fd, &i, sizeof(i));
                if (i) {
                    break;
                }
            }
            if (FD_ISSET(udev_fd, &fds)) {
                struct udev_device* dev = udev_monitor_receive_device(monitor);
                if (dev) {
                    const char* devnode = udev_device_get_devnode(dev);
                    enum rsvc_disc_type type = path_to_disc_type(devnode);
                    rsvc_send_disc(context, abbrev_dev(devnode), type);
                    udev_device_unref(dev);
                }
            }
		}
	}

    udev_monitor_unref(monitor);
	udev_unref(udev);
}

void rsvc_disc_watch(struct rsvc_disc_watch_callbacks callbacks) {
    struct rsvc_watch_context build_context = {
        .queue = dispatch_queue_create("net.sfiera.ripservice.disc", NULL),
        .callbacks = {
            .appeared = Block_copy(callbacks.appeared),
            .disappeared = Block_copy(callbacks.disappeared),
            .initialized = Block_copy(callbacks.initialized),
        },
    };
    struct rsvc_watch_context* context = memdup(&build_context, sizeof(build_context));
    dispatch_async(context->queue, ^{
        disc_watch(context);
        dispatch_release(context->queue);
        Block_release(context->callbacks.appeared);
        Block_release(context->callbacks.disappeared);
        Block_release(context->callbacks.initialized);
        free(context);
    });
}

bool rsvc_disc_eject(const char* path, rsvc_done_t fail) {
    bool ok = false;
    int fd;
    if (rsvc_opendev(path, O_RDONLY | O_NONBLOCK, 0, &fd, fail)) {
        if (ioctl(fd, CDROM_LOCKDOOR, 0) < 0) {
            rsvc_strerrorf(fail, __FILE__, __LINE__, "%s", path);
        } else if (ioctl(fd, CDROMEJECT) < 0) {
            rsvc_strerrorf(fail, __FILE__, __LINE__, "%s", path);
        } else {
            ok = true;
        }
        close(fd);
    }
    return ok;
}
