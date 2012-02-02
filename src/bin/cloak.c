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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#include <rsvc/tag.h>
#include <rsvc/flac.h>
#include <rsvc/vorbis.h>

enum short_flag {
    HELP        = 'h',
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

static void usage(const char* progname) {
    fprintf(stderr,
            "usage: %s [OPTIONS] FILE...\n"
            "\n"
            "Options:\n"
            "  -h, --help                display this help and exit\n"
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

typedef void (^op_t)(rsvc_tags_t, void (^done)(rsvc_error_t));

static void validate_name(const char* progname, char* name);
static void split_assignment(const char* progname, const char* assignment,
                             char** name, char** value);
static void auto_tag(rsvc_tags_t tags, void (^done)(rsvc_error_t));

static void cloak_main(int argc, char* const* argv) {
    const char* progname = strdup(basename(argv[0]));

    __block rsvc_option_callbacks_t callbacks;
    __block bool list = false;

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

          case VERSION:
            printf("cloak %s\n", RSVC_VERSION);
            exit(0);

          case LIST:
            list = true;
            return true;

          case DELETE:
            {
                char* tag_name = strdup(value());  // leak
                validate_name(progname, tag_name);
                add_op(^(rsvc_tags_t tags, void (^done)(rsvc_error_t)){
                    rsvc_tags_remove(tags, tag_name);
                    done(NULL);
                });
            }
            return true;

          case DELETE_ALL:
            add_op(^(rsvc_tags_t tags, void (^done)(rsvc_error_t)){
                rsvc_tags_clear(tags);
                done(NULL);
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
                    add_op(^(rsvc_tags_t tags, void (^done)(rsvc_error_t)){
                        rsvc_tags_remove(tags, tag_name);
                        rsvc_tags_add(tags, tag_name, tag_value);
                        done(NULL);
                    });
                } else {
                    add_op(^(rsvc_tags_t tags, void (^done)(rsvc_error_t)){
                        rsvc_tags_add(tags, tag_name, tag_value);
                        done(NULL);
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
                add_op(^(rsvc_tags_t tags, void (^done)(rsvc_error_t)){
                    rsvc_tags_remove(tags, tag_name(opt));
                    rsvc_tags_add(tags, tag_name(opt), tag_value);
                    done(NULL);
                });
            }
            return true;

          case AUTO:
            add_op(^(rsvc_tags_t tags, void (^done)(rsvc_error_t)){
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
    } else if ((ops == NULL) && !list) {
        callbacks.usage("no actions");
    }

    void (^done)(rsvc_error_t) = ^(rsvc_error_t error){
        if (error) {
            fprintf(stderr, "%s: %s\n", progname, error->message);
            exit(1);
        }
        exit(0);
    };

    rsvc_tags_t tags = rsvc_tags_create();
    __block void (^apply_file)(size_t) = ^(size_t i){
        if (i < nfiles) {
            const char* path = files[i];
            rsvc_flac_read_tags(path, tags, ^(rsvc_error_t error){
                if (error) {
                    done(error);
                    return;
                }

                __block void (^apply_op)(size_t) = ^(size_t j){
                    if (j < nops) {
                        ops[j](tags, ^(rsvc_error_t error){
                            if (error) {
                                done(error);
                                return;
                            }
                            apply_op(j + 1);
                        });
                    } else {
                        rsvc_flac_write_tags(path, tags, ^(rsvc_error_t error){
                            if (error) {
                                done(error);
                                return;
                            }

                            if (list) {
                                if (nfiles > 1) {
                                    printf("%s:\n", path);
                                }
                                rsvc_tags_each(tags, ^(const char* name, const char* value,
                                                       rsvc_stop_t stop){
                                    printf("%s=%s\n", name, value);
                                });
                            }
                            rsvc_tags_clear(tags);

                            if (list && (nfiles > 1) && ((i + 1) < nfiles)) {
                                printf("\n");
                            }
                            apply_file(i + 1);
                        });
                    }
                };
                apply_op(0);
            });
        } else {
            done(NULL);
        }
    };
    apply_file(0);

    dispatch_main();
}

static void auto_tag(rsvc_tags_t tags, void (^done)(rsvc_error_t)) {
    rsvc_errorf(done, __FILE__, __LINE__, "musicbrainz: not implemented");
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
