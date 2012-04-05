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

#include <rsvc/musicbrainz.h>

#include <Block.h>
#include <dispatch/dispatch.h>
#include <string.h>
#include <sys/time.h>

static void* memdup(const void* data, size_t size) {
    void* copy = malloc(size);
    memcpy(copy, data, size);
    return copy;
}

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
    if (!rsvc_tags_each(tags, ^(const char* name, const char* value, rsvc_stop_t stop){
        if (strcmp(name, tag_name) == 0) {
            stop();
        }
    })) {
        // Already have tag.
        return true;
    }

    char tag_value[1024];
    accessor(object, tag_value, 1024);
    return rsvc_tags_add(tags, fail, tag_name, tag_value);
}

static void mb4_query_cached(const char* discid, void (^done)(rsvc_error_t, Mb4Metadata)) {
    // MusicBrainz requires that requests be throttled to 1 per second.
    // In order to do so, we serialize all of our requests through a
    // single dispatch queue.
    static dispatch_queue_t cache_queue;
    static dispatch_queue_t throttle_queue;
    static dispatch_once_t init;
    dispatch_once(&init, ^{
        cache_queue = dispatch_queue_create(
            "net.sfiera.ripservice.musicbrainz-cache", NULL);
        throttle_queue = dispatch_queue_create(
            "net.sfiera.ripservice.musicbrainz-throttle", NULL);
    });

    struct cache_entry {
        char*               discid;
        Mb4Metadata         meta;
        struct cache_entry* next;
    };
    static struct cache_entry* cache = NULL;

    dispatch_async(cache_queue, ^{
        // Look for `discid` in the cache.
        //
        // TODO(sfiera): avoid starting multiple requests for the same
        // cache key.
        for (struct cache_entry* curr = cache; curr; curr = curr->next) {
            if (strcmp(discid, curr->discid) == 0) {
                rsvc_logf(1, "mb request in cache for %s", discid);
                done(NULL, curr->meta);
                return;
            }
        }

        rsvc_logf(1, "starting mb request for %s", discid);
        dispatch_async(throttle_queue, ^{
            // As soon as we enter the throttling queue, dispatch an
            // asynchronous request.  We'll sleep for the next second in the
            // throttling queue, blocking the next request from being
            // dispatched, but even if the request itself takes longer than
            // a second, we'll be free to start another.
            dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
                rsvc_logf(2, "sending mb request for %s", discid);
                Mb4Query q = mb4_query_new("ripservice " RSVC_VERSION, NULL, 0);
                char* param_names[] = {"inc"};
                char* param_values[] = {"artists+recordings"};
                Mb4Metadata meta = mb4_query_query(q, "discid", discid, NULL,
                                                   1, param_names, param_values);
                rsvc_logf(1, "received mb response for %s", discid);
                done(NULL, meta);
                dispatch_sync(cache_queue, ^{
                    struct cache_entry new_cache = {
                        .discid = strdup(discid),
                        .meta   = meta,
                        .next   = cache,
                    };
                    cache = memdup(&new_cache, sizeof new_cache);
                });
                mb4_query_delete(q);
            });

            // usleep() returns early if there is a signal.  Make sure that
            // we wait out the full thousand microseconds.
            int64_t exit_time = now_usecs() + 1000000ll;
            rsvc_logf(2, "throttling until %lld", exit_time);
            int64_t remaining;
            while ((remaining = exit_time - now_usecs()) > 0) {
                usleep(remaining);
            }
        });
    });
}

void rsvc_apply_musicbrainz_tags(rsvc_tags_t tags, rsvc_done_t done) {
    __block char* discid = 0;
    __block bool found_discid = false;
    __block int tracknumber = 0;
    __block bool found_tracknumber = false;
    if (!rsvc_tags_each(tags, ^(const char* name, const char* value, rsvc_stop_t stop){
        if (strcmp(name, RSVC_MUSICBRAINZ_DISCID) == 0) {
            found_discid = true;
            discid = strdup(value);
        } else if (strcmp(name, RSVC_TRACKNUMBER) == 0) {
            found_tracknumber = true;
            char* endptr;
            tracknumber = strtol(value, &endptr, 10);
            if (*endptr) {
                rsvc_errorf(done, __FILE__, __LINE__, "bad track number: %s", value);
                stop();
            }
        }
    })) {
        return;
    }
    if (!found_discid) {
        rsvc_errorf(done, __FILE__, __LINE__, "missing discid");
        return;
    } else if (!found_tracknumber) {
        rsvc_errorf(done, __FILE__, __LINE__, "missing track number");
        return;
    }

    mb4_query_cached(discid, ^(rsvc_error_t error, Mb4Metadata meta) {
        if (error) {
            done(error);
            goto cleanup;
        }

        Mb4Disc disc = mb4_metadata_get_disc(meta);
        Mb4ReleaseList rel_list = mb4_disc_get_releaselist(disc);
        for (int i = 0; i < mb4_release_list_size(rel_list); ++i) {
            Mb4Release release = mb4_release_list_item(rel_list, i);
            Mb4MediumList med_list = mb4_release_get_mediumlist(release);
            for (int j = 0; j < mb4_medium_list_size(med_list); ++j) {
                Mb4Medium medium = mb4_medium_list_item(med_list, j);
                if (!mb4_medium_contains_discid(medium, discid)) {
                    continue;
                }

                Mb4Track track = NULL;
                Mb4TrackList trk_list = mb4_medium_get_tracklist(medium);
                for (int k = 0; k < mb4_track_list_size(trk_list); ++k) {
                    Mb4Track tk = mb4_track_list_item(trk_list, k);
                    if (mb4_track_get_position(tk) == tracknumber) {
                        track = tk;
                        break;
                    }
                }
                if (track == NULL) {
                    continue;
                }
                Mb4Recording recording = mb4_track_get_recording(track);

                Mb4ArtistCredit artist_credit = mb4_release_get_artistcredit(release);
                Mb4NameCreditList crd_list = mb4_artistcredit_get_namecreditlist(
                        artist_credit);
                Mb4Artist artist = NULL;
                if (mb4_namecredit_list_size(crd_list) > 0) {
                    Mb4NameCredit credit = mb4_namecredit_list_item(crd_list, 0);
                    artist = mb4_namecredit_get_artist(credit);
                }

                if (set_mb_tag(tags, RSVC_TITLE, mb4_recording_get_title, recording, done)
                        && set_mb_tag(tags, RSVC_ARTIST, mb4_artist_get_name, artist, done)
                        && set_mb_tag(tags, RSVC_ALBUM, mb4_release_get_title, release, done)
                        && set_mb_tag(tags, RSVC_DATE, mb4_release_get_date, release, done)) {
                    done(NULL);
                }
                goto cleanup;
            }
        }

        rsvc_errorf(done, __FILE__, __LINE__, "discid not found: %s\n", discid);
cleanup:
        free(discid);
    });
}
