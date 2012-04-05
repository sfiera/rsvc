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

#include <Block.h>
#include <ctype.h>
#include <dispatch/dispatch.h>
#include <fcntl.h>
#include <libgen.h>
#include <musicbrainz4/mb4_c.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sysexits.h>

#include <rsvc/tag.h>
#include <rsvc/flac.h>
#include <rsvc/mp4.h>
#include <rsvc/vorbis.h>

enum short_flag {
    HELP        = 'h',
    VERBOSE     = 'v',
    VERSION     = 'V',

    LIST        = 'l',
    DELETE      = 'd',
    DELETE_ALL  = 'D',
    ADD         = -1,
    SET         = 's',

    ARTIST      = 'a',
    ALBUM       = 'A',
    TITLE       = 't',
    GENRE       = 'g',
    YEAR        = 'y',
    TRACK       = 'n',
    TRACK_TOTAL = 'N',
    DISC        = -2,
    DISC_TOTAL  = -3,
    AUTO        = -4,
};

struct long_flag {
    const char* name;
    enum short_flag value;
} kLongFlags[] = {
    {"help",        HELP},
    {"verbose",     VERBOSE},
    {"version",     VERSION},

    {"list",        LIST},
    {"delete",      DELETE},
    {"delete-all",  DELETE_ALL},
    {"add",         ADD},
    {"set",         SET},

    {"artist",      ARTIST},
    {"album",       ALBUM},
    {"title",       TITLE},
    {"genre",       GENRE},
    {"year",        YEAR},
    {"track",       TRACK},
    {"track-total", TRACK_TOTAL},
    {"disc",        DISC},
    {"disc-total",  DISC_TOTAL},

    {"auto",        AUTO},

    {NULL},
};

typedef enum list_mode {
    LIST_MODE_NONE = 0,
    LIST_MODE_SHORT,
    LIST_MODE_LONG,
} list_mode_t;

int verbosity = 0;
static void logf(int level, const char* format, ...) {
    static dispatch_queue_t log_queue;
    static dispatch_once_t log_queue_init;
    dispatch_once(&log_queue_init, ^{
        log_queue = dispatch_queue_create("net.sfiera.ripservice.log", NULL);
    });
    if (level <= verbosity) {
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
        vasprintf(&message, format, vl);
        va_end(vl);

        fprintf(stderr, "%s log: %s\n", time, message);
        free(message);
    }
}

static void usage(const char* progname) {
    fprintf(stderr,
            "usage: %s [OPTIONS] FILE...\n"
            "\n"
            "Options:\n"
            "  -h, --help                display this help and exit\n"
            "  -v, --verbose             more verbose logging\n"
            "  -V, --version             show version and exit\n"
            "\n"
            "  Basic:\n"
            "    -l, --list              list all tags\n"
            "    -d, --delete NAME       delete all tags with name NAME\n"
            "    -D, --delete-all        delete all tags\n"
            "        --add NAME=VALUE    add VALUE to the tag with name NAME\n"
            "    -s, --set NAME=VALUE    set the tag with name NAME to VALUE\n"
            "\n"
            "  Shorthand:\n"
            "    -a, --artist ARTIST     set the artist name\n"
            "    -A, --album ALBUM       set the album name\n"
            "    -t, --title TITLE       set the track title\n"
            "    -g, --genre GENRE       set the genre\n"
            "    -y, --year YEAR         set the release date\n"
            "    -n, --track NUM         set the track number\n"
            "    -N, --track-total NUM   set the track total\n"
            "        --disc NUM          set the disc number\n"
            "        --disc-total NUM    set the disc total\n"
            "\n"
            "  MusicBrainz:\n"
            "        --auto              fetch missing tags from MusicBrainz\n"
            "                            (requires that MUSICBRAINZ_DISCID be set)\n",
            progname);
}

static const char* tag_name(int opt) {
    switch (opt) {
      case ARTIST:          return "ARTIST";
      case ALBUM:           return "ALBUM";
      case TITLE:           return "TITLE";
      case GENRE:           return "GENRE";
      case YEAR:            return "YEAR";
      case TRACK:           return "TRACKNUMBER";
      case TRACK_TOTAL:     return "TRACKTOTAL";
      case DISC:            return "DISCNUMBER";
      case DISC_TOTAL:      return "DISCTOTAL";
    }
    abort();
}

typedef void (^op_t)(rsvc_tags_t, rsvc_done_t done);

static void tag_files(size_t nfiles, char** files,
                      size_t nops, op_t* ops,
                      list_mode_t list_mode, rsvc_done_t done);
static void tag_file(const char* path, size_t nops, op_t* ops,
                     list_mode_t list_mode, rsvc_done_t done);
static void apply_ops(rsvc_tags_t tags, size_t nops, op_t* ops,
                      rsvc_done_t done);

static void validate_name(const char* progname, char* name);
static void split_assignment(const char* progname, const char* assignment,
                             char** name, char** value);
static void auto_tag(rsvc_tags_t tags, rsvc_done_t done);

static void cloak_main(int argc, char* const* argv) {
    const char* progname = strdup(basename(argv[0]));

    __block rsvc_option_callbacks_t callbacks;
    __block list_mode_t list_mode = LIST_MODE_NONE;

    __block int nops = 0;
    __block op_t* ops = NULL;
    void (^add_op)(op_t op) = ^(op_t op) {
        op_t* new_ops = calloc(nops + 1, sizeof(op_t));
        for (size_t i = 0; i < nops; ++i) {
            new_ops[i] = ops[i];
        }
        new_ops[nops++] = Block_copy(op);
        if (ops) {
            free(ops);
        }
        ops = new_ops;
    };

    __block int nfiles = 0;
    __block char** files = NULL;
    void (^add_file)(const char*) = ^(const char* path){
        char** new_files = calloc(nfiles + 1, sizeof(char*));
        for (size_t i = 0; i < nfiles; ++i) {
            new_files[i] = files[i];
        }
        new_files[nfiles++] = strdup(path);
        if (files) {
            free(files);
        }
        files = new_files;
    };

    bool (^option)(int opt, char* (^value)()) = ^bool (int opt, char* (^value)()){
        switch (opt) {
          case HELP:
            usage(progname);
            exit(0);

          case VERBOSE:
            ++verbosity;
            return true;

          case VERSION:
            printf("cloak %s\n", RSVC_VERSION);
            exit(0);

          case LIST:
            list_mode = LIST_MODE_SHORT;
            return true;

          case DELETE:
            {
                char* tag_name = strdup(value());  // leak
                validate_name(progname, tag_name);
                add_op(^(rsvc_tags_t tags, rsvc_done_t done){
                    if (rsvc_tags_remove(tags, tag_name, done)) {
                        done(NULL);
                    }
                });
            }
            return true;

          case DELETE_ALL:
            add_op(^(rsvc_tags_t tags, rsvc_done_t done){
                if (rsvc_tags_clear(tags, done)) {
                    done(NULL);
                }
            });
            return true;

          case ADD:
          case SET:
            {
                char* tag_name;  // leak
                char* tag_value;  // leak
                split_assignment(progname, value(), &tag_name, &tag_value);
                validate_name(progname, tag_name);
                if (opt == SET) {
                    add_op(^(rsvc_tags_t tags, rsvc_done_t done){
                        if (rsvc_tags_remove(tags, tag_name, done)
                                && rsvc_tags_add(tags, done, tag_name, tag_value)) {
                            done(NULL);
                        }
                    });
                } else {
                    add_op(^(rsvc_tags_t tags, rsvc_done_t done){
                        if (rsvc_tags_add(tags, done, tag_name, tag_value)) {
                            done(NULL);
                        }
                    });
                }
            }
            return true;

          case ARTIST:
          case ALBUM:
          case TITLE:
          case GENRE:
          case YEAR:
          case TRACK:
          case TRACK_TOTAL:
          case DISC:
          case DISC_TOTAL:
            {
                char* tag_value = strdup(value());  // leak
                add_op(^(rsvc_tags_t tags, rsvc_done_t done){
                    if (rsvc_tags_remove(tags, tag_name(opt), done)
                            && rsvc_tags_add(tags, done, tag_name(opt), tag_value)) {
                        done(NULL);
                    }
                });
            }
            return true;

          case AUTO:
            add_op(^(rsvc_tags_t tags, rsvc_done_t done){
                auto_tag(tags, done);
            });
            return true;
        }
        return false;
    };

    callbacks.short_option = ^bool (char opt, char* (^value)()){
        return option(opt, value);
    };

    callbacks.long_option = ^bool (char* opt, char* (^value)()){
        for (struct long_flag* flag = kLongFlags; flag->name; ++flag) {
            if (strcmp(opt, flag->name) == 0) {
                return option(flag->value, value);
            }
        }
        return false;
    };

    callbacks.argument = ^bool (char* arg){
        add_file(arg);
        return true;
    };

    callbacks.usage = ^(const char* message, ...){
        if (message) {
            fprintf(stderr, "%s: ", progname);
            va_list vl;
            va_start(vl, message);
            vfprintf(stderr, message, vl);
            va_end(vl);
            fprintf(stderr, "\n");
        }
        usage(progname);
        exit(EX_USAGE);
    };

    rsvc_options(argc, argv, &callbacks);

    if (files == NULL) {
        callbacks.usage("no input files");
    } else if ((ops == NULL) && !list_mode) {
        callbacks.usage("no actions");
    }

    if ((nfiles > 1) && (list_mode == LIST_MODE_SHORT)) {
        list_mode = LIST_MODE_LONG;
    }
    tag_files(nfiles, files, nops, ops, list_mode, ^(rsvc_error_t error){
        if (error) {
            fprintf(stderr, "%s: %s\n", progname, error->message);
            exit(1);
        }
        exit(0);
    });

    dispatch_main();
}

static void tag_files(size_t nfiles, char** files,
                      size_t nops, op_t* ops,
                      list_mode_t list_mode, rsvc_done_t done) {
    if (nfiles == 0) {
        done(NULL);
        return;
    }
    tag_file(files[0], nops, ops, list_mode, ^(rsvc_error_t error){
        if (error) {
            done(error);
            return;
        }
        if (list_mode && (nfiles > 1)) {
            printf("\n");
        }
        tag_files(nfiles - 1, files + 1, nops, ops, list_mode, done);
    });
}

struct rsvc_container_type {
    size_t magic_size;
    const char* magic;
    void (*read_tags)(const char* path, void (^done)(rsvc_tags_t, rsvc_error_t));
    struct rsvc_container_type* next;
};
typedef struct rsvc_container_type* rsvc_container_type_t;

struct rsvc_container_types {
    size_t max_magic_size;
    rsvc_container_type_t head;
};

static struct rsvc_container_type mp4_container = {
    .magic_size = 12,
    .magic = "????ftypM4A ",
    .read_tags = rsvc_mp4_read_tags,
};
static struct rsvc_container_type flac_container = {
    .magic_size = 4,
    .magic = "fLaC",
    .read_tags = rsvc_flac_read_tags,
    .next = &mp4_container,
};
static struct rsvc_container_types container_types = {
    .max_magic_size = 12,
    .head = &flac_container,
};

static bool detect_container_type(int fd, rsvc_container_type_t* container, rsvc_done_t fail) {
    // Don't open directories.
    struct stat st;
    if (fstat(fd, &st) < 0) {
        rsvc_strerrorf(fail, __FILE__, __LINE__, NULL);
        return false;
    }
    if (st.st_mode & S_IFDIR) {
        rsvc_errorf(fail, __FILE__, __LINE__, "is a directory");
        return false;
    }

    // Detect file type by magic number.
    uint8_t* data = alloca(container_types.max_magic_size);
    ssize_t size = read(fd, data, container_types.max_magic_size);
    if (size < 0) {
        rsvc_strerrorf(fail, __FILE__, __LINE__, NULL);
        return false;
    }
    for (rsvc_container_type_t curr = container_types.head; curr; curr = curr->next) {
        if (size < curr->magic_size) {
            continue;
        }
        for (size_t i = 0; i < curr->magic_size; ++i) {
            if ((curr->magic[i] != '?') && (curr->magic[i] != data[i])) {
                goto next_container_type;
            }
        }
        *container = curr;
        return true;
next_container_type:
        ;
    }
    rsvc_errorf(fail, __FILE__, __LINE__, "couldn't detect file type");
    return false;
}

static void tag_file(const char* path, size_t nops, op_t* ops,
                     list_mode_t list_mode, rsvc_done_t done) {
    int fd;
    if (!rsvc_open(path, O_RDONLY, 0644, &fd, done)) {
        return;
    }
    // From here on, prepend the file name to the error message, and
    // close the file when we're done.
    done = ^(rsvc_error_t error) {
        close(fd);
        if (error) {
            rsvc_errorf(done, error->file, error->lineno, "%s: %s", path, error->message);
        } else {
            done(NULL);
        }
    };

    rsvc_container_type_t container;
    if (!detect_container_type(fd, &container, done)) {
        return;
    }

    container->read_tags(path, ^(rsvc_tags_t tags, rsvc_error_t error){
        if (error) {
            done(error);
            return;
        }
        apply_ops(tags, nops, ops, ^(rsvc_error_t error){
            if (error) {
                done(error);
                return;
            }
            rsvc_tags_save(tags, ^(rsvc_error_t error){
                if (error) {
                    done(error);
                    return;
                }
                if (list_mode == LIST_MODE_LONG) {
                    printf("%s:\n", path);
                }
                if (list_mode) {
                    rsvc_tags_each(tags, ^(const char* name, const char* value,
                                           rsvc_stop_t stop){
                        printf("%s=%s\n", name, value);
                    });
                }
                rsvc_tags_destroy(tags);
                done(NULL);
            });
        });
    });
}

static void apply_ops(rsvc_tags_t tags, size_t nops, op_t* ops,
                      rsvc_done_t done) {
    if (nops == 0) {
        done(NULL);
        return;
    }
    ops[0](tags, ^(rsvc_error_t error){
        if (error) {
            done(error);
            return;
        }
        apply_ops(tags, nops - 1, ops + 1, done);
    });
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

static int64_t now_usecs() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (1000000ll * tv.tv_sec) + tv.tv_usec;
}

static void* memdup(const void* data, size_t size) {
    void* copy = malloc(size);
    memcpy(copy, data, size);
    return copy;
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

    done = Block_copy(done);
    dispatch_async(cache_queue, ^{
        // Look for `discid` in the cache.
        //
        // TODO(sfiera): avoid starting multiple requests for the same
        // cache key.
        for (struct cache_entry* curr = cache; curr; curr = curr->next) {
            if (strcmp(discid, curr->discid) == 0) {
                logf(1, "mb request in cache for %s", discid);
                done(NULL, curr->meta);
                Block_release(done);
                return;
            }
        }

        logf(1, "starting mb request for %s", discid);
        dispatch_async(throttle_queue, ^{
            // As soon as we enter the throttling queue, dispatch an
            // asynchronous request.  We'll sleep for the next second in the
            // throttling queue, blocking the next request from being
            // dispatched, but even if the request itself takes longer than
            // a second, we'll be free to start another.
            dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
                logf(2, "sending mb request for %s", discid);
                Mb4Query q = mb4_query_new("ripservice " RSVC_VERSION, NULL, 0);
                char* param_names[] = {"inc"};
                char* param_values[] = {"artists+recordings"};
                Mb4Metadata meta = mb4_query_query(q, "discid", discid, NULL,
                                                   1, param_names, param_values);
                logf(1, "received mb response for %s", discid);
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
                Block_release(done);
            });

            // usleep() returns early if there is a signal.  Make sure that
            // we wait out the full thousand microseconds.
            int64_t exit_time = now_usecs() + 1000000ll;
            logf(2, "throttling until %lld", exit_time);
            int64_t remaining;
            while ((remaining = exit_time - now_usecs()) > 0) {
                usleep(remaining);
            }
        });
    });
}

static void auto_tag(rsvc_tags_t tags, rsvc_done_t done) {
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
        Block_release(done);
        free(discid);
    });
}

static void validate_name(const char* progname, char* name) {
    if (name[strspn(name, "ABCDEFGHIJKLMNOPQRSTUVWXYZ_"
                          "abcdefghijklmnopqrstuvwxyz")] != '\0') {
        fprintf(stderr, "%s: invalid tag name: %s\n", progname, name);
        exit(EX_USAGE);
    }
    for (char* p = name; *p; ++p) {
        *p = toupper(*p);
    }
}

static void split_assignment(const char* progname, const char* assignment,
                             char** name, char** value) {
    char* eq = strchr(assignment, '=');
    if (eq == NULL) {
        fprintf(stderr, "%s: missing tag value: %s\n", progname, *name);
        exit(EX_USAGE);
    }
    *name = strndup(assignment, eq - assignment);
    *value = strdup(eq + 1);
}

int main(int argc, char* const* argv) {
    cloak_main(argc, argv);
    return EX_OK;
}
