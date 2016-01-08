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

#define _BSD_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <rsvc/musicbrainz.h>

#include <Block.h>
#include <dispatch/dispatch.h>
#include <musicbrainz5/mb5_c.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "common.h"

static int64_t now_usecs() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (1000000ll * tv.tv_sec) + tv.tv_usec;
}

static bool set_mb_tag(rsvc_tags_t tags, const char* tag_name,
                       int (*accessor)(void*, char*, int), void* object,
                       rsvc_done_t fail) {
    if (object == NULL) {
        return true;
    }
    for (rsvc_tags_iter_t it = rsvc_tags_begin(tags); rsvc_next(it); ) {
        if (strcmp(it->name, tag_name) == 0) {
            // Already have tag.
            rsvc_break(it);
            return true;
        }
    }

    char tag_value[1024];
    accessor(object, tag_value, 1024);
    return rsvc_tags_add(tags, fail, tag_name, tag_value);
}

static bool mb5_query_cached(const char* discid, Mb5Metadata* meta, rsvc_done_t fail) {
    (void)fail;  // TODO(sfiera): handle failure conditions?

    // MusicBrainz requires that requests be throttled to 1 per second.
    // In order to do so, we serialize all of our requests through a
    // single dispatch queue.
    static dispatch_queue_t cache_queue;
    static dispatch_semaphore_t throttle;
    static dispatch_once_t init;
    dispatch_once(&init, ^{
        cache_queue = dispatch_queue_create(
            "net.sfiera.ripservice.musicbrainz-cache", NULL);
        throttle = dispatch_semaphore_create(1);
    });

    struct cache_entry {
        char*               discid;
        Mb5Metadata         meta;
        struct cache_entry* next;
    };
    static struct cache_entry* cache = NULL;

    __block bool result = true;
    dispatch_sync(cache_queue, ^{
        // Look for `discid` in the cache.
        //
        // TODO(sfiera): avoid starting multiple requests for the same
        // cache key.
        for (struct cache_entry* curr = cache; curr; curr = curr->next) {
            if (strcmp(discid, curr->discid) == 0) {
                rsvc_logf(1, "mb request in cache for %s", discid);
                *meta = curr->meta;
                result = true;
                return;
            }
        }

        rsvc_logf(1, "starting mb request for %s", discid);
        dispatch_semaphore_wait(throttle, DISPATCH_TIME_FOREVER);
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            // usleep() returns early if there is a signal.  Make sure that
            // we wait out the full thousand microseconds.
            int64_t exit_time = now_usecs() + 1000000ll;
            rsvc_logf(2, "throttling until %lld", (long long)exit_time);
            int64_t remaining;
            while ((remaining = exit_time - now_usecs()) > 0) {
                usleep(remaining);
            }
            dispatch_semaphore_signal(throttle);
        });

        rsvc_logf(2, "sending mb request for %s", discid);
        Mb5Query q = mb5_query_new("ripservice " RSVC_VERSION, NULL, 0);
        char* param_names[] = {"inc"};
        char* param_values[] = {"artists+recordings"};
        Mb5Metadata response = mb5_query_query(q, "discid", discid, NULL,
                                             1, param_names, param_values);
        rsvc_logf(1, "received mb response for %s", discid);
        *meta = response;
        struct cache_entry new_cache = {
            .discid = strdup(discid),
            .meta   = response,
            .next   = cache,
        };
        cache = memdup(&new_cache, sizeof new_cache);
        mb5_query_delete(q);
    });
    return result;
}

bool rsvc_apply_musicbrainz_tags(rsvc_tags_t tags, rsvc_done_t fail) {
    bool success = false;
    char* discid = 0;
    bool found_discid = false;
    int tracknumber = 0;
    bool found_tracknumber = false;
    for (rsvc_tags_iter_t it = rsvc_tags_begin(tags); rsvc_next(it); ) {
        if (strcmp(it->name, RSVC_MUSICBRAINZ_DISCID) == 0) {
            found_discid = true;
            discid = strdup(it->value);
        } else if (strcmp(it->name, RSVC_TRACKNUMBER) == 0) {
            found_tracknumber = true;
            char* endptr;
            tracknumber = strtol(it->value, &endptr, 10);
            if (*endptr) {
                rsvc_errorf(fail, __FILE__, __LINE__, "bad track number: %s", it->value);
                rsvc_break(it);
                goto cleanup;
            }
        }
    }

    if (!found_discid) {
        rsvc_errorf(fail, __FILE__, __LINE__, "missing discid");
        goto cleanup;
    } else if (!found_tracknumber) {
        rsvc_errorf(fail, __FILE__, __LINE__, "missing track number");
        goto cleanup;
    }

    Mb5Metadata meta;
    if (!mb5_query_cached(discid, &meta, fail)) {
        goto cleanup;
    }

    Mb5Disc disc = mb5_metadata_get_disc(meta);
    Mb5ReleaseList rel_list = mb5_disc_get_releaselist(disc);
    for (int i = 0; i < mb5_release_list_size(rel_list); ++i) {
        Mb5Release release = mb5_release_list_item(rel_list, i);
        Mb5MediumList med_list = mb5_release_get_mediumlist(release);
        for (int j = 0; j < mb5_medium_list_size(med_list); ++j) {
            Mb5Medium medium = mb5_medium_list_item(med_list, j);
            if (!mb5_medium_contains_discid(medium, discid)) {
                continue;
            }

            Mb5Track track = NULL;
            Mb5TrackList trk_list = mb5_medium_get_tracklist(medium);
            for (int k = 0; k < mb5_track_list_size(trk_list); ++k) {
                Mb5Track tk = mb5_track_list_item(trk_list, k);
                if (mb5_track_get_position(tk) == tracknumber) {
                    track = tk;
                    break;
                }
            }
            if (track == NULL) {
                continue;
            }
            Mb5Recording recording = mb5_track_get_recording(track);

            Mb5ArtistCredit artist_credit = mb5_release_get_artistcredit(release);
            Mb5NameCreditList crd_list = mb5_artistcredit_get_namecreditlist(artist_credit);
            Mb5Artist artist = NULL;
            if (mb5_namecredit_list_size(crd_list) > 0) {
                Mb5NameCredit credit = mb5_namecredit_list_item(crd_list, 0);
                artist = mb5_namecredit_get_artist(credit);
            }

            success =
                set_mb_tag(tags, RSVC_TITLE, mb5_recording_get_title, recording, fail)
                && set_mb_tag(tags, RSVC_ARTIST, mb5_artist_get_name, artist, fail)
                && set_mb_tag(tags, RSVC_ALBUM, mb5_release_get_title, release, fail)
                && set_mb_tag(tags, RSVC_DATE, mb5_release_get_date, release, fail);
            goto cleanup;
        }
    }

    rsvc_errorf(fail, __FILE__, __LINE__, "discid not found: %s\n", discid);
cleanup:
    if (discid) {
        free(discid);
    }
    return success;
}
