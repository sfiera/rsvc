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

#include "common.h"

#include <dispatch/dispatch.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sysexits.h>
#include <time.h>

int rsvc_vasprintf(char** value, const char* format, va_list ap) {
    int size = vsnprintf(NULL, 0, format, ap);
    if (size < 0) {
        return size;
    }
    *value = malloc(size + 1);
    if (!*value) {
        return -1;
    }
    return vsnprintf(*value, size + 1, format, ap);
}

void rsvc_errorf(rsvc_done_t callback,
                 const char* file, int lineno, const char* format, ...) {
    char* message;
    va_list vl;
    va_start(vl, format);
    rsvc_vasprintf(&message, format, vl);
    va_end(vl);
    struct rsvc_error error = {message, file, lineno};
    callback(&error);
    free(message);
}

void rsvc_strerrorf(rsvc_done_t callback,
                    const char* file, int lineno, const char* format, ...) {
    char* strerror = NULL;
    size_t buflen = strerror_r(errno, strerror, 0);
    strerror = malloc(buflen);
    strerror_r(errno, strerror, buflen);
    if (format) {
        char* message;
        va_list vl;
        va_start(vl, format);
        rsvc_vasprintf(&message, format, vl);
        va_end(vl);
        rsvc_errorf(callback, file, lineno, "%s: %s", message, strerror);
        free(message);
    } else {
        rsvc_errorf(callback, file, lineno, "%s", strerror);
    }
    free(strerror);
}

rsvc_error_t rsvc_error_clone(rsvc_error_t error) {
    if (error) {
        char* message = strdup(error->message);
        char* file = strdup(error->file);
        int lineno = error->lineno;
        struct rsvc_error copy = {message, file, lineno};
        return memdup(&copy, sizeof(copy));
    } else {
        return error;
    }
}

void rsvc_error_destroy(rsvc_error_t error) {
    if (error) {
        free(error->message);
        free(error);
    }
}

void rsvc_error_async(dispatch_queue_t queue, rsvc_error_t error, rsvc_done_t done) {
    if (error) {
        char* message = strdup(error->message);
        const char* file = error->file;
        int lineno = error->lineno;
        dispatch_async(queue, ^{
            struct rsvc_error error = {message, file, lineno};
            done(&error);
            free(message);
        });
    } else {
        dispatch_async(queue, ^{
            done(NULL);
        });
    }
}

int rsvc_verbosity = 0;
void rsvc_logf(int level, const char* format, ...) {
    static dispatch_queue_t log_queue;
    static dispatch_once_t log_queue_init;
    dispatch_once(&log_queue_init, ^{
        log_queue = dispatch_queue_create("net.sfiera.ripservice.log", NULL);
    });
    if (level <= rsvc_verbosity) {
        time_t t;
        time(&t);
        struct tm tm;
        gmtime_r(&t, &tm);
        char strtime[20];
        strftime(strtime, 20, "%F %T", &tm);
        const char* time = strtime;

        char* message;
        va_list vl;
        va_start(vl, format);
        rsvc_vasprintf(&message, format, vl);
        va_end(vl);

        fprintf(stderr, "%s log: %s\n", time, message);
        free(message);
    }
}

void* memdup(const void* data, size_t size) {
    void* copy = malloc(size);
    memcpy(copy, data, size);
    return copy;
}
