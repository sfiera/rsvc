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
#include <fcntl.h>
#include <libgen.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sysexits.h>

#include <rsvc/tag.h>
#include <rsvc/flac.h>
#include <rsvc/mp4.h>
#include <rsvc/musicbrainz.h>
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

static const char* get_tag_name(int opt) {
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
    bool            delete_all_tags;
    string_list_t   delete_tags;

    string_list_t   add_tag_names;
    string_list_t   add_tag_values;

    bool            auto_mode;
    list_mode_t     list_mode;
};
typedef struct ops* ops_t;

static void tag_files(size_t nfiles, char** files, ops_t ops, rsvc_done_t done);
static void tag_file(const char* path, ops_t ops, rsvc_done_t done);
static void apply_ops(rsvc_tags_t tags, ops_t ops, rsvc_done_t done);

static void validate_name(const char* progname, char* name);
static void split_assignment(const char* progname, const char* assignment,
                             char** name, char** value);

static void cloak_main(int argc, char* const* argv) {
    const char* progname = strdup(basename(argv[0]));

    __block rsvc_option_callbacks_t callbacks;

    __block bool no_actions = true;
    __block struct ops ops = {
        .auto_mode          = false,
        .list_mode          = LIST_MODE_NONE,
        .delete_all_tags    = false,
    };
    __block string_list_t files = {};

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
            ops.list_mode = LIST_MODE_SHORT;
            no_actions = false;
            return true;

          case DELETE:
            validate_name(progname, value());
            add_string(&ops.delete_tags, value());
            no_actions = false;
            return true;

          case DELETE_ALL:
            ops.delete_all_tags = true;
            no_actions = false;
            return true;

          case ADD:
          case SET:
            {
                char* tag_name;
                char* tag_value;
                split_assignment(progname, value(), &tag_name, &tag_value);
                validate_name(progname, tag_name);
                if (opt == SET) {
                    add_string(&ops.delete_tags, tag_name);
                }
                add_string(&ops.add_tag_names, tag_name);
                add_string(&ops.add_tag_values, tag_value);
                free(tag_name);
                free(tag_value);
                no_actions = false;
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
                const char* tag_name = get_tag_name(opt);
                char* tag_value = strdup(value());
                add_string(&ops.delete_tags, tag_name);
                add_string(&ops.add_tag_names, tag_name);
                add_string(&ops.add_tag_values, tag_value);
                free(tag_value);
                no_actions = false;
            }
            return true;

          case AUTO:
            ops.auto_mode = true;
            no_actions = false;
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
        usage(progname);
        exit(EX_USAGE);
    };

    rsvc_options(argc, argv, &callbacks);

    if (files.nstrings == 0) {
        callbacks.usage("no input files");
    } else if (no_actions) {
        callbacks.usage("no actions");
    }

    if ((files.nstrings > 1) && (ops.list_mode == LIST_MODE_SHORT)) {
        ops.list_mode = LIST_MODE_LONG;
    }
    tag_files(files.nstrings, files.strings, &ops, ^(rsvc_error_t error){
        if (error) {
            fprintf(stderr, "%s: %s\n", progname, error->message);
            exit(1);
        }
        exit(0);
    });

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

struct rsvc_container_type {
    size_t magic_size;
    const char* magic;
    void (*read_tags)(const char* path, void (^done)(rsvc_tags_t, rsvc_error_t));
    struct rsvc_container_type* next;
};
typedef struct rsvc_container_type* rsvc_container_type_t;

struct rsvc_container_types {
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

    for (rsvc_container_type_t curr = container_types.head; curr; curr = curr->next) {
        // Detect file type by magic number.
        uint8_t* data = malloc(curr->magic_size);
        ssize_t size = pread(fd, data, curr->magic_size, 0);
        if (size < 0) {
            free(data);
            rsvc_strerrorf(fail, __FILE__, __LINE__, NULL);
            return false;
        } else if (size < curr->magic_size) {
            goto next_container_type;
        }
        for (size_t i = 0; i < curr->magic_size; ++i) {
            if ((curr->magic[i] != '?') && (curr->magic[i] != data[i])) {
                goto next_container_type;
            }
        }
        *container = curr;
        free(data);
        return true;
next_container_type:
        free(data);
    }
    rsvc_errorf(fail, __FILE__, __LINE__, "couldn't detect file type");
    return false;
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

    rsvc_container_type_t container;
    if (!detect_container_type(fd, &container, done)) {
        return;
    }

    container->read_tags(path, ^(rsvc_tags_t tags, rsvc_error_t error){
        if (error) {
            done(error);
            return;
        }
        apply_ops(tags, ops, ^(rsvc_error_t error){
            if (error) {
                rsvc_tags_destroy(tags);
                done(error);
                return;
            }
            rsvc_tags_save(tags, ^(rsvc_error_t error){
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
    });
}

static void apply_ops(rsvc_tags_t tags, ops_t ops, rsvc_done_t done) {
    if (ops->delete_all_tags) {
        if (!rsvc_tags_clear(tags, done)) {
            return;
        }
    } else {
        for (size_t i = 0; i < ops->delete_tags.nstrings; ++i) {
            if (!rsvc_tags_remove(tags, ops->delete_tags.strings[i], done)) {
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

    if (ops->auto_mode) {
        rsvc_apply_musicbrainz_tags(tags, done);
    } else {
        done(NULL);
    }
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
