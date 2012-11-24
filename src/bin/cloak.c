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

#include <ctype.h>
#include <dispatch/dispatch.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sysexits.h>
#include <unistd.h>

#include <rsvc/tag.h>
#include <rsvc/flac.h>
#include <rsvc/id3.h>
#include <rsvc/mp4.h>
#include <rsvc/musicbrainz.h>
#include <rsvc/vorbis.h>

#include "../rsvc/options.h"
#include "../rsvc/unix.h"

#define DEFAULT_FORMAT "./%b/%A/%d%k%t"

enum short_flag {
    HELP            = 'h',
    DRY_RUN         = 'n',
    VERBOSE         = 'v',
    VERSION         = 'V',

    LIST            = 'l',
    REMOVE          = 'r',
    REMOVE_ALL      = 'R',
    ADD             = 'x',
    SET             = 's',

    ARTIST          = 'a',
    ALBUM           = 'A',
    ALBUM_ARTIST    = 'b',
    TITLE           = 't',
    GENRE           = 'g',
    DATE            = 'y',
    TRACK           = 'k',
    TRACK_TOTAL     = 'K',
    DISC            = 'd',
    DISC_TOTAL      = 'D',

    AUTO            = -1,

    MOVE            = 'm',
    FORMAT_PATH     = 'f',
};

struct long_flag {
    const char* name;
    enum short_flag value;
} kLongFlags[] = {
    {"help",            HELP},
    {"dry-run",         DRY_RUN},
    {"verbose",         VERBOSE},
    {"version",         VERSION},

    {"list",            LIST},
    {"remove",          REMOVE},
    {"remove-all",      REMOVE_ALL},
    {"add",             ADD},
    {"set",             SET},

    {"artist",          ARTIST},
    {"album",           ALBUM},
    {"album-artist",    ALBUM},
    {"title",           TITLE},
    {"genre",           GENRE},
    {"date",            DATE},
    {"track",           TRACK},
    {"track-total",     TRACK_TOTAL},
    {"disc",            DISC},
    {"disc-total",      DISC_TOTAL},

    {"auto",            AUTO},

    {"move",            MOVE},
    {"format-path",     FORMAT_PATH},

    {NULL},
};

typedef enum list_mode {
    LIST_MODE_NONE = 0,
    LIST_MODE_SHORT,
    LIST_MODE_LONG,
} list_mode_t;

static void cloak_usage(const char* progname) {
    fprintf(stderr,
            "usage: %s [OPTIONS] FILE...\n"
            "\n"
            "Options:\n"
            "  -h, --help                display this help and exit\n"
            "  -n, --dry-run             validate inputs but don't save changes\n"
            "  -v, --verbose             more verbose logging\n"
            "  -V, --version             show version and exit\n"
            "\n"
            "  Basic:\n"
            "    -l, --list              list all tags\n"
            "    -r, --remove NAME       remove all tags with name NAME\n"
            "    -R, --remove-all        remove all tags\n"
            "    -x, --add NAME=VALUE    add VALUE to the tag with name NAME\n"
            "    -s, --set NAME=VALUE    set the tag with name NAME to VALUE\n"
            "\n"
            "  Shorthand:\n"
            "    -a, --artist ARTIST     set the artist name\n"
            "    -A, --album ALBUM       set the album name\n"
            "    -b, --album-artist AA   set the album artist name\n"
            "    -t, --title TITLE       set the track title\n"
            "    -g, --genre GENRE       set the genre\n"
            "    -y, --date DATE         set the release date\n"
            "    -k, --track NUM         set the track number\n"
            "    -K, --track-total NUM   set the track total\n"
            "    -d, --disc NUM          set the disc number\n"
            "    -D, --disc-total NUM    set the disc total\n"
            "\n"
            "  MusicBrainz:\n"
            "        --auto              fetch missing tags from MusicBrainz\n"
            "                            (requires that MUSICBRAINZ_DISCID be set)\n"
            "\n"
            "  Organization:\n"
            "    -m, --move              move file according to new tags\n"
            "    -f, --format-path PATH  format string for --move (default %s)\n",
            progname, DEFAULT_FORMAT);
}

static const char* get_tag_name(int opt) {
    switch (opt) {
      case ARTIST:          return RSVC_ARTIST;
      case ALBUM:           return RSVC_ALBUM;
      case ALBUM_ARTIST:    return RSVC_ALBUMARTIST;
      case TITLE:           return RSVC_TITLE;
      case GENRE:           return RSVC_GENRE;
      case DATE:            return RSVC_DATE;
      case TRACK:           return RSVC_TRACKNUMBER;
      case TRACK_TOTAL:     return RSVC_TRACKTOTAL;
      case DISC:            return RSVC_DISCNUMBER;
      case DISC_TOTAL:      return RSVC_DISCTOTAL;
    }
    return NULL;
}

struct string_list {
    size_t nstrings;
    char** strings;
};
typedef struct string_list string_list_t;
static void add_string(string_list_t* list, const char* string) {
    char** new_strings = calloc(list->nstrings + 1, sizeof(char*));
    for (size_t i = 0; i < list->nstrings; ++i) {
        new_strings[i] = list->strings[i];
    }
    new_strings[list->nstrings++] = strdup(string);
    if (list->strings) {
        free(list->strings);
    }
    list->strings = new_strings;
}

struct ops {
    bool            remove_all_tags;
    string_list_t   remove_tags;

    string_list_t   add_tag_names;
    string_list_t   add_tag_values;

    bool            auto_mode;

    bool            dry_run;
    list_mode_t     list_mode;

    bool            move_mode;
    char*           move_format;
};
typedef struct ops* ops_t;

static void tag_files(size_t nfiles, char** files, ops_t ops, rsvc_done_t done);
static void tag_file(const char* path, ops_t ops, rsvc_done_t done);
static bool any_actions(ops_t ops);
static int ops_mode(ops_t ops);
static void apply_ops(rsvc_tags_t tags, const char* path, ops_t ops, rsvc_done_t done);
static void format_path(const char* format, rsvc_tags_t tags, const char* extension,
                        void (^done)(rsvc_error_t error, char* path));

static void validate_name(const char* progname, char* name);
static bool split_assignment(const char* assignment, char** name, char** value,
                             rsvc_done_t fail);

static void cloak_main(int argc, char* const* argv) {
    const char* progname = strdup(basename(argv[0]));
    rsvc_done_t fail = ^(rsvc_error_t error){
        if (error) {
            fprintf(stderr, "%s: %s (%s:%d)\n",
                    progname, error->message, error->file, error->lineno);
            exit(1);
        }
        exit(0);
    };

    rsvc_flac_format_register();
    rsvc_vorbis_format_register();
    rsvc_mp4_format_register();
    rsvc_id3_format_register();

    __block rsvc_option_callbacks_t callbacks;

    __block struct ops ops = {
        .auto_mode          = false,
        .list_mode          = LIST_MODE_NONE,
        .remove_all_tags    = false,
    };
    __block string_list_t files = {};

    bool (^option)(int opt, char* (^value)()) = ^bool (int opt, char* (^value)()){
        switch (opt) {
          case HELP:
            cloak_usage(progname);
            exit(0);

          case DRY_RUN:
            ops.dry_run = true;
            return true;

          case VERBOSE:
            ++rsvc_verbosity;
            return true;

          case VERSION:
            printf("cloak %s\n", RSVC_VERSION);
            exit(0);

          case LIST:
            ops.list_mode = LIST_MODE_SHORT;
            return true;

          case REMOVE:
            validate_name(progname, value());
            add_string(&ops.remove_tags, value());
            return true;

          case REMOVE_ALL:
            ops.remove_all_tags = true;
            return true;

          case ADD:
          case SET:
            {
                char* tag_name;
                char* tag_value;
                if (!split_assignment(value(), &tag_name, &tag_value, fail)) {
                    return true;
                }
                validate_name(progname, tag_name);
                if (opt == SET) {
                    add_string(&ops.remove_tags, tag_name);
                }
                add_string(&ops.add_tag_names, tag_name);
                add_string(&ops.add_tag_values, tag_value);
                free(tag_name);
                free(tag_value);
            }
            return true;

          case ARTIST:
          case ALBUM:
          case ALBUM_ARTIST:
          case TITLE:
          case GENRE:
          case DATE:
          case TRACK:
          case TRACK_TOTAL:
          case DISC:
          case DISC_TOTAL:
            {
                const char* tag_name = get_tag_name(opt);
                char* tag_value = strdup(value());
                add_string(&ops.remove_tags, tag_name);
                add_string(&ops.add_tag_names, tag_name);
                add_string(&ops.add_tag_values, tag_value);
                free(tag_value);
            }
            return true;

          case AUTO:
            ops.auto_mode = true;
            return true;

          case MOVE:
            ops.move_mode = true;
            return true;

          case FORMAT_PATH:
            if (ops.move_format) {
                free(ops.move_format);
            }
            ops.move_format = strdup(value());
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
        add_string(&files, arg);
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
        cloak_usage(progname);
        exit(EX_USAGE);
    };

    rsvc_options(argc, argv, &callbacks);

    if (files.nstrings == 0) {
        callbacks.usage("no input files");
    } else if (!any_actions(&ops)) {
        callbacks.usage("no actions");
    }

    if ((files.nstrings > 1) && (ops.list_mode == LIST_MODE_SHORT)) {
        ops.list_mode = LIST_MODE_LONG;
    }
    tag_files(files.nstrings, files.strings, &ops, fail);

    dispatch_main();
}

static void tag_files(size_t nfiles, char** files, ops_t ops, rsvc_done_t done) {
    if (nfiles == 0) {
        done(NULL);
        return;
    }
    tag_file(files[0], ops, ^(rsvc_error_t error){
        if (error) {
            done(error);
            return;
        }
        if (ops->list_mode && (nfiles > 1)) {
            printf("\n");
        }
        tag_files(nfiles - 1, files + 1, ops, done);
    });
}

static void tag_file(const char* path, ops_t ops, rsvc_done_t done) {
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

    rsvc_tag_format_t format;
    if (!rsvc_tag_format_detect(fd, &format, done)) {
        return;
    }

    format->open_tags(path, ops_mode(ops), ^(rsvc_tags_t tags, rsvc_error_t error){
        if (error) {
            done(error);
            return;
        }
        apply_ops(tags, path, ops, ^(rsvc_error_t error){
            if (error) {
                rsvc_tags_destroy(tags);
                done(error);
                return;
            }
            if (ops->list_mode == LIST_MODE_LONG) {
                printf("%s:\n", path);
            }
            if (ops->list_mode) {
                rsvc_tags_each(tags, ^(const char* name, const char* value,
                                       rsvc_stop_t stop){
                    printf("%s=%s\n", name, value);
                });
            }
            rsvc_tags_destroy(tags);
            done(NULL);
        });
    });
}

typedef struct format_node* format_node_t;
typedef struct format_list* format_list_t;

struct format_node {
    int     type;
    size_t  size;
    char    data[0];
};

struct format_list {
    format_node_t   head;
    format_node_t   tail;
};

static bool parse_format(const char* format,
                         rsvc_done_t fail,
                         void (^block)(int type, const char* data, size_t size)) {
    while (*format) {
        size_t literal_size = strcspn(format, "%/");
        if (literal_size) {
            block(0, format, literal_size);
        }
        format += literal_size;

        switch (*format) {
          case '\0':
            break;
          case '/':
            literal_size = strspn(format, "/");
            block(0, format, literal_size);
            format += literal_size;
            break;
          case '%':
            ++format;
            if (*format == '%') {
                block(0, format, 1);
                ++format;
            } else if (get_tag_name(*format)) {
                // Check that it's a valid option, then add it.
                block(*format, NULL, 0);
                ++format;
            } else if ((' ' <= *format) && (*format < 0x80)) {
                rsvc_errorf(fail, __FILE__, __LINE__, "invalid format code %%%c", *format);
                return false;
            } else {
                rsvc_errorf(fail, __FILE__, __LINE__, "invalid format code %%\\%o", *format);
                return false;
            }
        }
    }
    return true;
}

// Append `src` to `*dst`, ensuring we do not overrun `*dst`.
//
// Advance `*dst` and `*dst_size` to point to the remainder of the
// string.
static void clipped_cat(char** dst, size_t* dst_size, const char* src, size_t src_size,
                        size_t* size_needed) {
    rsvc_logf(4, "src=%s, size=%zu", src, src_size);
    if (size_needed) {
        *size_needed += src_size;
    }
    if (*dst_size < src_size) {
        src_size = *dst_size;
    }
    memcpy(*dst, src, src_size);
    *dst += src_size;
    *dst_size -= src_size;
}

static void escape_for_path(char* string) {
    for (char* ch = string; *ch; ++ch) {
        // Allow non-ASCII characters and whitelisted ASCII.
        if (*ch & 0x80) {
            continue;
        } else if (strchr("ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                          "abcdefghijklmnopqrstuvwxyz"
                          "0123456789"
                          " #%&'()*+,-@[]^_{|}~", *ch)) {
            continue;
        }
        *ch = '_';
    }
}

static bool any_tags(rsvc_tags_t tags, const char* tag_name) {
    return !rsvc_tags_each(tags, ^(const char* name, const char* value, rsvc_stop_t stop){
        if (strcmp(name, tag_name) == 0) {
            stop();
        }
    });
}

static size_t max_precision(rsvc_tags_t tags, const char* tag_name, size_t minimum) {
    __block size_t result = minimum;
    rsvc_tags_each(tags, ^(const char* name, const char* value, rsvc_stop_t stop){
        if ((strcmp(name, tag_name) == 0) && (strlen(value) > result)) {
            result = strlen(value);
        }
    });
    return result;
}

// Return true if `str` matches "0|[1-9][0-9]*".
static bool is_canonical_int(const char* str) {
    if (strcmp(str, "0") == 0) {
        return true;
    } else if (('1' <= *str) && (*str <= '9')) {
        for (++str; *str; ++str) {
            if ((*str < '0') || ('9' < *str)) {
                return false;
            }
        }
        return true;
    }
    return false;
}

// TODO(sfiera): more robust extension-finding.
static const char* get_extension(const char* path) {
    const char* extension = strrchr(path, '.');
    if (!extension || strchr(extension, '/')) {
        return NULL;
    } else {
        return extension;
    }
}

enum snpath_state {
    SNPATH_INITIAL = 0,
    SNPATH_CONTENT,
    SNPATH_NO_CONTENT,
};

struct snpath_context {
    enum snpath_state   state;
    int                 type;
};

static bool snpathf(char* data, size_t size, size_t* size_needed,
                    const char* format, rsvc_tags_t tags, const char* extension,
                    rsvc_done_t fail) {
    bool add_trailing_nul = size;
    __block char* dst = data;
    __block size_t dst_size = size;
    if (add_trailing_nul) {
        --dst_size;
    }
    if (size_needed) {
        *size_needed = 0;
    }

    __block struct snpath_context ctx = { };
    if (!parse_format(format, fail, ^(int type, const char* src, size_t src_size){
        if (type == 0) {
            if (!src_size) {
                return;
            } else if (*src == '/') {
                if (ctx.state != SNPATH_NO_CONTENT) {
                    clipped_cat(&dst, &dst_size, src, src_size, size_needed);
                }
                ctx.state = SNPATH_NO_CONTENT;
            } else {
                clipped_cat(&dst, &dst_size, src, src_size, size_needed);
                ctx.state = SNPATH_CONTENT;
            }
            ctx.type = type;
        } else {
            const char* separator = ", ";
            const char* prefix = "";
            if ((type == TRACK) && (ctx.type == DISC)) {
                prefix = "-";
            } else if (ctx.type) {
                prefix = " ";
            }

            size_t precision = 0;
            if (type == TRACK) {
                precision = max_precision(tags, RSVC_TRACKTOTAL, 2);
            } else if (type == DISC) {
                precision = max_precision(tags, RSVC_DISCTOTAL, 1);
            } else if ((type == ALBUM_ARTIST) && !any_tags(tags, RSVC_ALBUMARTIST)) {
                type = ARTIST;
            }

            __block size_t count = 0;
            rsvc_tags_each(tags, ^(const char* name, const char* value, rsvc_stop_t stop){
                if (strcmp(name, get_tag_name(type)) == 0) {
                    if (!*value) {
                        return;
                    }
                    if (count++ == 0) {
                        clipped_cat(&dst, &dst_size, prefix, strlen(prefix), size_needed);
                    } else {
                        clipped_cat(&dst, &dst_size, separator, strlen(separator), size_needed);
                    }
                    if (precision && is_canonical_int(value)) {
                        for (size_t i = strlen(value); i < precision; ++i) {
                            clipped_cat(&dst, &dst_size, "0", 1, size_needed);
                        }
                    }
                    char* escaped = strdup(value);
                    escape_for_path(escaped);
                    clipped_cat(&dst, &dst_size, escaped, strlen(escaped), size_needed);
                    free(escaped);
                }
            });
            if (count) {
                ctx.state = SNPATH_CONTENT;
                ctx.type = type;
            } else if (ctx.state == SNPATH_INITIAL) {
                ctx.state = SNPATH_NO_CONTENT;
            }
        }
    })) {
        return false;
    };

    if (extension) {
        clipped_cat(&dst, &dst_size, extension, strlen(extension), size_needed);
    }

    if (add_trailing_nul) {
        *dst = '\0';
    }
    return true;
}

static void format_path(const char* format, rsvc_tags_t tags, const char* extension,
                        void (^done)(rsvc_error_t error, char* path)) {
    rsvc_done_t fail = ^(rsvc_error_t error){
        done(error, NULL);
    };

    size_t size;
    if (!snpathf(NULL, 0, &size, format, tags, extension, fail)) {
        return;
    }
    char* new_path = malloc(size + 1);
    if (!snpathf(new_path, size + 1, NULL, format, tags, extension, fail)) {
        return;
    }
    done(NULL, new_path);
    free(new_path);
}

static bool any_actions(ops_t ops) {
    return ops->remove_all_tags
        || ops->remove_tags.nstrings
        || ops->add_tag_names.nstrings
        || ops->auto_mode
        || ops->list_mode
        || ops->move_mode;
}

static int ops_mode(ops_t ops) {
    if (ops->remove_all_tags
            || ops->remove_tags.nstrings
            || ops->add_tag_names.nstrings
            || ops->auto_mode) {
        return RSVC_TAG_RDWR;
    } else {
        return RSVC_TAG_RDONLY;
    }
}

typedef struct segment* segment_t;
struct segment {
    const char* data;
    size_t size;
};

static void segment_noalloc(const char* path, segment_t* segments, size_t* nsegments) {
    size_t i = 0;
    while (*path) {
        size_t span = strcspn(path, " ./");
        if (!span) {
            span = 1;
        }
        if (segments) {
            (*segments)[i].data = path;
            (*segments)[i].size = span;
        }
        ++i;
        path += span;
    }
    if (nsegments) {
        *nsegments = i;
    }
}

static void segment(const char* path, segment_t* segments, size_t* nsegments) {
    segment_noalloc(path, NULL, nsegments);
    *segments = malloc(*nsegments * sizeof(struct segment));
    segment_noalloc(path, segments, NULL);
}

static bool _find_next_common_segment(
        segment_t aseg, size_t a_equal,
        segment_t bseg, size_t nbseg, size_t* b_equal) {
    for (size_t i = 0; (i <= a_equal) && (i <= nbseg); ++i) {
        if ((aseg[a_equal].size == bseg[i].size)
                && (strncmp(aseg[a_equal].data, bseg[i].data, aseg[a_equal].size) == 0)) {
            if (strchr(" ./", *(aseg[a_equal].data)) == NULL) {
                *b_equal = i;
                return true;
            }
        }
    }
    return false;
}

static void backtrack_single_segments(
        segment_t srcseg, size_t* src_equal,
        segment_t dstseg, size_t* dst_equal) {
    while ((*src_equal > 0) && (*dst_equal > 0)) {
        segment_t src = srcseg + (*src_equal) - 1;
        segment_t dst = dstseg + (*dst_equal) - 1;
        if ((src->size != dst->size) || (src->size != 1)
                || (*(src->data) != *(dst->data))) {
            return;
        }
        --(*src_equal);
        --(*dst_equal);
    }
}

static void find_next_common_segment(
        segment_t srcseg, size_t nsrcseg, size_t* src_equal,
        segment_t dstseg, size_t ndstseg, size_t* dst_equal) {
    size_t maxseg = (nsrcseg > ndstseg) ? nsrcseg : ndstseg;
    for (size_t i = 1; i < maxseg; ++i) {
        size_t k;
        if ((i < nsrcseg) && _find_next_common_segment(srcseg, i, dstseg, ndstseg, &k)) {
            *src_equal = i;
            *dst_equal = k;
            backtrack_single_segments(srcseg, src_equal, dstseg, dst_equal);
            return;
        }
        if ((i < ndstseg) && _find_next_common_segment(dstseg, i, srcseg, nsrcseg, &k)) {
            *src_equal = k;
            *dst_equal = i;
            backtrack_single_segments(srcseg, src_equal, dstseg, dst_equal);
            return;
        }
    }
    *src_equal = nsrcseg;
    *dst_equal = ndstseg;
}

static void skip_advance_segment(segment_t* seg, size_t* nseg) {
    ++(*seg);
    --(*nseg);
}

static void print_advance_segment(segment_t* seg, size_t* nseg) {
    char* copy = strndup((*seg)->data, (*seg)->size);
    printf("%s", copy);
    free(copy);
    skip_advance_segment(seg, nseg);
}

static void print_rename(const char* src, const char* dst) {
    segment_t srcseg;
    size_t nsrcseg;
    segment(src, &srcseg, &nsrcseg);
    segment_t dstseg;
    size_t ndstseg;
    segment(dst, &dstseg, &ndstseg);
    segment_t srcseg_save = srcseg;
    segment_t dstseg_save = dstseg;

    printf("rename: ");
    while (nsrcseg && ndstseg) {
        // Segments are equal.
        if ((srcseg->size == dstseg->size)
                && (strncmp(srcseg->data, dstseg->data, srcseg->size) == 0)) {
            print_advance_segment(&srcseg, &nsrcseg);
            skip_advance_segment(&dstseg, &ndstseg);
            continue;
        }

        // Segments are not equal.
        size_t src_equal, dst_equal;
        find_next_common_segment(srcseg, nsrcseg, &src_equal,
                                 dstseg, ndstseg, &dst_equal);
        printf("{");
        for (size_t i = 0; i < src_equal; ++i) {
            print_advance_segment(&srcseg, &nsrcseg);
        }
        printf(" => ");
        for (size_t i = 0; i < dst_equal; ++i) {
            print_advance_segment(&dstseg, &ndstseg);
        }
        printf("}");
    }
    printf("\n");

    free(srcseg_save);
    free(dstseg_save);
}

bool same_file(const char* x, const char* y) {
    void (^ignore)(rsvc_error_t) = ^(rsvc_error_t error){};
    int a_fd, b_fd;
    if (rsvc_open(x, O_RDONLY, 0644, &a_fd, ignore)
            && rsvc_open(y, O_RDONLY, 0644, &b_fd, ignore)) {
        struct stat a_st, b_st;
        if ((fstat(a_fd, &a_st) >= 0) && (fstat(b_fd, &b_st) >= 0)) {
            return (a_st.st_dev == b_st.st_dev)
                && (a_st.st_ino == b_st.st_ino);
        }
    }
    return false;
}

static void apply_ops(rsvc_tags_t tags, const char* path, ops_t ops, rsvc_done_t done) {
    if (ops->remove_all_tags) {
        if (!rsvc_tags_clear(tags, done)) {
            return;
        }
    } else {
        for (size_t i = 0; i < ops->remove_tags.nstrings; ++i) {
            if (!rsvc_tags_remove(tags, ops->remove_tags.strings[i], done)) {
                return;
            }
        }
    }

    for (size_t i = 0; i < ops->add_tag_names.nstrings; ++i) {
        const char* name = ops->add_tag_names.strings[i];
        const char* value = ops->add_tag_values.strings[i];
        if (!rsvc_tags_add(tags, done, name, value)) {
            return;
        }
    }

    if (ops->move_mode) {
        done = ^(rsvc_error_t error){
            if (error) {
                done(error);
            } else {
                const char* format = ops->move_format ? ops->move_format : DEFAULT_FORMAT;
                const char* extension = get_extension(path);
                format_path(format, tags, extension, ^(rsvc_error_t error, char* new_path){
                    if (error) {
                        done(error);
                    } else if (ops->dry_run) {
                        if (!same_file(path, new_path)) {
                            print_rename(path, new_path);
                        }
                        done(NULL);
                    } else {
                        rsvc_dirname(path, ^(const char* parent){
                            rsvc_dirname(new_path, ^(const char* new_parent){
                                if (rsvc_makedirs(new_parent, 0755, done)
                                    && rsvc_mv(path, new_path, done)) {
                                    rsvc_trimdirs(parent);
                                    done(NULL);
                                }
                            });
                        });
                    }
                });
            }
        };
    }
    if (!ops->dry_run) {
        done = ^(rsvc_error_t error){
            if (error) {
                done(error);
            } else {
                rsvc_tags_save(tags, done);
            }
        };
    }
    if (ops->auto_mode) {
        done = ^(rsvc_error_t error){
            if (error) {
                done(error);
            } else {
                rsvc_apply_musicbrainz_tags(tags, done);
            }
        };
    }
    done(NULL);
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

static bool split_assignment(const char* assignment, char** name, char** value,
                             rsvc_done_t fail) {
    char* eq = strchr(assignment, '=');
    if (eq == NULL) {
        rsvc_errorf(fail, __FILE__, __LINE__, "missing tag value: %s", assignment);
        return false;
    }
    *name = strndup(assignment, eq - assignment);
    *value = strdup(eq + 1);
    return true;
}

int main(int argc, char* const* argv) {
    cloak_main(argc, argv);
    return EX_OK;
}
