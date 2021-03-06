//
// This file is part of Rip Service.
//
// Copyright (C) 2014 Chris Pickel <sfiera@sfzmail.com>
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

#include "cloak.h"

static bool help_option(const char* progname);
static bool formats_option(const char* progname);
static bool verbosity_option();
static bool version_option();
static bool literal_tag_option(ops_t ops, const char* tag_name, const char* tag_value);
static bool tag_option(ops_t ops, rsvc_option_value_f get_value, enum short_flag flag,
                       rsvc_done_t fail);
static bool image_option(ops_t ops, rsvc_option_value_f get_value, enum short_flag flag,
                         rsvc_done_t fail);
static bool all_path_option(ops_t ops, rsvc_option_value_f get_value, rsvc_done_t fail);
static bool type_path_option(ops_t ops, const char* flag, rsvc_option_value_f get_value,
                             bool* matched, rsvc_done_t fail);
static bool shorthand_option(ops_t ops, char opt, rsvc_option_value_f get_value, rsvc_done_t fail);
static bool check_options(string_list_t files, ops_t ops, rsvc_done_t fail);

struct rsvc_long_option_name kLongFlags[] = {
    {"help",                HELP},
    {"formats",             FORMATS},
    {"dry-run",             DRY_RUN},
    {"verbose",             VERBOSE},
    {"version",             VERSION},

    {"list",                LIST},
    {"list-images",         LIST_IMAGES},
    {"remove",              REMOVE},
    {"remove-all",          REMOVE_ALL},
    {"add",                 ADD},
    {"set",                 SET},

    {"artist",              RSVC_CODE_ARTIST},
    {"album",               RSVC_CODE_ALBUM},
    {"albumartist",         RSVC_CODE_ALBUMARTIST},
    {"title",               RSVC_CODE_TITLE},
    {"genre",               RSVC_CODE_GENRE},
    {"grouping",            RSVC_CODE_GROUPING},
    {"date",                RSVC_CODE_DATE},
    {"tracknumber",         RSVC_CODE_TRACKNUMBER},
    {"tracktotal",          RSVC_CODE_TRACKTOTAL},
    {"discnumber",          RSVC_CODE_DISCNUMBER},
    {"disctotal",           RSVC_CODE_DISCTOTAL},
    {"movie",               MOVIE},
    {"tv",                  TV},
    {"show",                RSVC_CODE_SHOW},
    {"episodenumber",       RSVC_CODE_EPISODENUMBER},
    {"episodetotal",        RSVC_CODE_EPISODETOTAL},
    {"seasonnumber",        RSVC_CODE_SEASONNUMBER},
    {"seasontotal",         RSVC_CODE_SEASONTOTAL},

    {"image",               IMAGE},
    {"write-image",         WRITE_IMAGE},
    {"select-image",        SELECT_IMAGE},
    {"remove-image",        REMOVE_IMAGE},
    {"remove-all-images",   REMOVE_ALL_IMAGES},
    {"add-image",           ADD_IMAGE},

    {"auto",                AUTO},

    {"move",                MOVE},
    {"path",                PATH},

    {NULL},
};

static void add_string(string_list_t list, const char* string) {
    struct string_list_node node = {strdup(string)};
    string_list_node_t copy = memdup(&node, sizeof(node));
    RSVC_LIST_PUSH(list, copy);
}

bool cloak_options(int argc, char* const* argv, ops_t ops, string_list_t files, rsvc_done_t fail) {
    const char* progname = strdup(basename(argv[0]));
    __block struct rsvc_option_callbacks callbacks;

    callbacks.short_option = ^bool (int32_t opt, rsvc_option_value_f get_value, rsvc_done_t fail){
        switch (opt) {
          case HELP:                return help_option(progname);
          case FORMATS:             return formats_option(progname);
          case DRY_RUN:             return rsvc_boolean_option(&ops->dry_run);
          case VERBOSE:             return verbosity_option();
          case VERSION:             return version_option();
          case LIST:                return rsvc_boolean_option(&ops->list_tags);
          case LIST_IMAGES:         return rsvc_boolean_option(&ops->list_images);
          case ADD:                 return tag_option(ops, get_value, opt, fail);
          case SET:                 return tag_option(ops, get_value, opt, fail);
          case REMOVE:              return tag_option(ops, get_value, opt, fail);
          case REMOVE_ALL:          return rsvc_boolean_option(&ops->remove_all_tags);
          case MOVIE:               return literal_tag_option(ops, RSVC_MEDIAKIND, "Movie");
          case TV:                  return literal_tag_option(ops, RSVC_MEDIAKIND, "TV Show");
          case IMAGE:               return image_option(ops, get_value, opt, fail);
          case WRITE_IMAGE:         return image_option(ops, get_value, opt, fail);
          case WRITE_IMAGE_DEFAULT: return image_option(ops, get_value, opt, fail);
          case SELECT_IMAGE:        return rsvc_integer_option(&ops->image_index, get_value, fail);
          case REMOVE_IMAGE:        return image_option(ops, get_value, opt, fail);
          case REMOVE_ALL_IMAGES:   return rsvc_boolean_option(&ops->remove_all_images);
          case ADD_IMAGE:           return image_option(ops, get_value, opt, fail);
          case AUTO:                return rsvc_boolean_option(&ops->auto_mode);
          case MOVE:                return rsvc_boolean_option(&ops->move_mode);
          case PATH:                return all_path_option(ops, get_value, fail);
          default:                  return shorthand_option(ops, opt, get_value, fail);
        }
    };

    callbacks.long_option = ^bool (char* opt, rsvc_option_value_f get_value, rsvc_done_t fail){
        bool matched;
        if (!type_path_option(ops, opt, get_value, &matched, fail)) {
            return false;
        }
        return matched
            || rsvc_long_option(kLongFlags, callbacks.short_option, opt, get_value, fail);
    };

    callbacks.argument = ^bool (char* arg, rsvc_done_t fail){
        (void)fail;
        add_string(files, arg);
        return true;
    };

    if (!(rsvc_options(argc, argv, &callbacks, fail)
          && check_options(files, ops, fail))) {
        return false;
    }

    if (ops->list_tags || ops->list_images) {
        ops->list_mode = (files->head != files->tail) ? LIST_MODE_LONG : LIST_MODE_SHORT;
    }
    return true;
}

int cloak_mode(ops_t ops) {
    if (ops->remove_all_tags
            || ops->remove_tags.head
            || ops->add_tag_names.head
            || ops->remove_all_images
            || ops->add_images.head
            || ops->remove_images.head
            || ops->auto_mode) {
        return RSVC_TAG_RDWR;
    } else if (ops->list_tags
            || ops->list_images
            || ops->write_images.head
            || ops->move_mode) {
        return RSVC_TAG_RDONLY;
    } else {
        return -1;
    }
}

static bool help_option(const char* progname) {
    errf(
            "usage: %s [OPTIONS] FILE...\n"
            "\n"
            "Options:\n"
            "  -h, --help                display this help and exit\n"
            "      --formats             list supported audio and image formats\n"
            "  -n, --dry-run             validate inputs but don't save changes\n"
            "  -v, --verbose             more verbose logging\n"
            "  -V, --version             show version and exit\n"
            "\n"
            "  Basic:\n"
            "    -l, --list              list all tags\n"
            "    -L, --list-images       list all images\n"
            "    -r, --remove NAME       remove all tags with name NAME\n"
            "    -R, --remove-all        remove all tags\n"
            "    -x, --add NAME=VALUE    add VALUE to the tag with name NAME\n"
            "    -s, --set NAME=VALUE    set the tag with name NAME to VALUE\n"
            "\n"
            "  Shorthand:\n"
            "    -a, --artist ARTIST     set the artist name\n"
            "    -A, --album ALBUM       set the album name\n"
            "    -@, --album-artist AA   set the album artist name\n"
            "    -t, --title TITLE       set the track title\n"
            "    -g, --genre GENRE       set the genre\n"
            "    -G, --grouping GROUP    set the grouping\n"
            "    -y, --date DATE         set the release date\n"
            "    -k, --track NUM         set the track number\n"
            "    -K, --track-total NUM   set the track total\n"
            "    -d, --disc NUM          set the disc number\n"
            "    -D, --disc-total NUM    set the disc total\n"
            "        --movie             set MEDIAKIND=Movie\n"
            "        --tv                set MEDIAKIND=TV Show\n"
            "    -S, --show NUM          set the show name\n"
            "    -e, --episode NUM       set the episode number\n"
            "    -E, --episode-total NUM set the episode total\n"
            "    -c, --season NUM        set the season number\n"
            "    -C, --season-total NUM  set the season total\n"
            "\n"
            "  Image:\n"
            "    -i, --image PNG|JPG     set the embedded image by path\n"
            "    -o, --write-image PATH  write the embedded image to a path\n"
            "                            (default MUSIC-DIR/cover.EXT)\n"
            "    -O                      same as --write-image=''\n"
            "    -I, --select-image N    select an image for --{write,remove}-image\n"
            "        --remove-image      remove the embedded image\n"
            "        --remove-all-images remove all embedded images\n"
            "        --add-image PNG|JPG add an embedded image by path\n"
            "\n"
            "  MusicBrainz:\n"
            "        --auto              fetch missing tags from MusicBrainz\n"
            "                            (requires that MUSICBRAINZ_DISCID be set)\n"
            "\n"
            "  Organization:\n"
            "    -m, --move              move file according to new tags\n"
            "    -p, --path PATH         format string for --move (default %s)\n"
            "        --TYPE-path PATH    override --path by type of file:\n"
            "                            audio, video, tv, movie\n",
            progname, DEFAULT_PATH);
    exit(0);
}

static bool formats_option(const char* progname) {
    (void)progname;
    enum rsvc_format_group format_groups[3] = {RSVC_AUDIO, RSVC_VIDEO, RSVC_IMAGE};
    for (int i = 0; i < 3; ++i) {
        enum rsvc_format_group format_group = format_groups[i];
        outf("%s:", rsvc_format_group_name(format_group));
        rsvc_formats_foreach(fmt) {
            if ((fmt->open_tags || (format_group == RSVC_IMAGE))
                && (fmt->format_group == format_group)) {
                outf(" %s", fmt->name);
            }
        }
        outf("\n");
    }
    exit(0);
}

static bool verbosity_option() {
    ++rsvc_verbosity;
    return true;
}

static bool version_option() {
    outf("cloak %s\n", RSVC_VERSION);
    exit(0);
}

static bool validate_name(char* name, rsvc_done_t fail) {
    if (name[strspn(name, "ABCDEFGHIJKLMNOPQRSTUVWXYZ_"
                          "abcdefghijklmnopqrstuvwxyz")] != '\0') {
        rsvc_errorf(fail, __FILE__, __LINE__, "invalid tag name: %s", name);
        return false;
    }
    for (char* p = name; *p; ++p) {
        *p = toupper(*p);
    }
    return true;
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

static bool literal_tag_option(ops_t ops, const char* tag_name, const char* tag_value) {
    add_string(&ops->remove_tags, tag_name);
    add_string(&ops->add_tag_names, tag_name);
    add_string(&ops->add_tag_values, tag_value);
    return true;
}

static bool tag_option(ops_t ops, rsvc_option_value_f get_value, enum short_flag flag,
                       rsvc_done_t fail) {
    if (flag == REMOVE) {
        char* value;
        if (!(get_value(&value, fail)
              && validate_name(value, fail))) {
            return false;
        }
        add_string(&ops->remove_tags, value);
        return true;
    }

    char* tag_name;
    char* tag_value;
    char* value;
    if (!(get_value(&value, fail)
          && split_assignment(value, &tag_name, &tag_value, fail)
          && validate_name(tag_name, fail))) {
        return false;
    }
    if (flag == SET) {
        add_string(&ops->remove_tags, tag_name);
    }
    add_string(&ops->add_tag_names, tag_name);
    add_string(&ops->add_tag_values, tag_value);
    free(tag_name);
    free(tag_value);
    return true;
}

static bool image_option(ops_t ops, rsvc_option_value_f get_value, enum short_flag flag,
                         rsvc_done_t fail) {
    if ((flag == IMAGE) || (flag == ADD_IMAGE)) {
        char* path;
        FILE* file;
        rsvc_format_t format;
        if (!(get_value(&path, fail)
              && rsvc_open(path, O_RDONLY, 0644, &file, fail)
              && rsvc_format_detect(path, file, &format, fail))) {
            return false;
        }
        if (!format->image_info) {
            rsvc_errorf(fail, __FILE__, __LINE__, "%s: not an image file", path);
            return false;
        }
        if (flag == IMAGE) {
            ops->remove_all_images = true;
        }
        struct add_image_list_node node = {path, file, format};
        add_image_list_node_t copy = memdup(&node, sizeof(node));
        RSVC_LIST_PUSH(&ops->add_images, copy);
        return true;
    } else if ((flag == WRITE_IMAGE) || (flag == WRITE_IMAGE_DEFAULT)) {
        char* path = NULL;
        if (flag == WRITE_IMAGE) {
            if (!get_value(&path, fail)) {
                return false;
            }
        }
        struct write_image_list_node node = {ops->image_index, path};
        write_image_list_node_t copy = memdup(&node, sizeof(node));
        RSVC_LIST_PUSH(&ops->write_images, copy);
        return true;
    } else if (flag == REMOVE_IMAGE) {
        struct remove_image_list_node node = {ops->image_index};
        remove_image_list_node_t copy = memdup(&node, sizeof(node));
        RSVC_LIST_PUSH(&ops->remove_images, copy);
        return true;
    }
    rsvc_errorf(fail, __FILE__, __LINE__, "internal error");
    return false;
}

bool starts_with(const char* str, const char* prefix) {
    size_t str_len = strlen(str);
    size_t prefix_len = strlen(prefix);
    return str_len >= prefix_len
        && (memcmp(str, prefix, prefix_len) == 0);
}

bool ends_with(const char* str, const char* suffix) {
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    return str_len >= suffix_len
        && (memcmp(str + str_len - suffix_len, suffix, suffix_len) == 0);
}

static bool push_path(ops_t ops, format_path_list_node_t node, rsvc_option_value_f get_value,
                      rsvc_done_t fail) {
    char* value;
    if (!get_value(&value, fail)
        || !rsvc_tags_validate_strf(value, fail)) {
        return false;
    }
    node->path = strdup(value);
    RSVC_LIST_PUSH(&ops->paths, memdup(node, sizeof(*node)));
    return true;
}

static bool all_path_option(ops_t ops, rsvc_option_value_f get_value, rsvc_done_t fail) {
    struct format_path_list_node node = {};
    node.priority = FPATH_ALL;
    return push_path(ops, &node, get_value, fail);
}

static bool type_path_option(ops_t ops, const char* flag, rsvc_option_value_f get_value,
                             bool* matched, rsvc_done_t fail) {
    *matched = false;
    if (ends_with(flag, "-path")) {
        struct format_path_list_node node = {};
        size_t flag_len = strlen(flag);  // "--path"
        char* what = memdup(flag, flag_len - 4);
        what[flag_len - 5] = '\0';

        static const enum rsvc_format_group groups[] = {RSVC_AUDIO, RSVC_VIDEO};
        for (int i = 0; i < (sizeof groups / sizeof groups[0]); ++i) {
            if (strcmp(what, rsvc_format_group_name(groups[i])) == 0) {
                free(what);
                node.priority = FPATH_GROUP;
                node.group = groups[i];
                *matched = true;
                return push_path(ops, &node, get_value, fail);
            }
        }

        static const char mediakinds[][2][16] = {
            {"tv",     "TV Show"},
            {"movie",  "Movie"},
        };
        for (int i = 0; i < (sizeof mediakinds / sizeof mediakinds[0]); ++i) {
            if (strcmp(what, mediakinds[i][0]) == 0) {
                free(what);
                node.priority = FPATH_MEDIAKIND;
                node.mediakind = mediakinds[i][1];
                *matched = true;
                return push_path(ops, &node, get_value, fail);
            }
        }

        free(what);
    }
    return true;
}

static bool shorthand_option(ops_t ops, char opt, rsvc_option_value_f get_value,
        rsvc_done_t fail) {
    if (rsvc_tag_code_get(opt)) {
        const char* tag_name = rsvc_tag_code_get(opt);
        char* value;
        if (!get_value(&value, fail)) {
            return false;
        }
        char* tag_value = strdup(value);
        add_string(&ops->remove_tags, tag_name);
        add_string(&ops->add_tag_names, tag_name);
        add_string(&ops->add_tag_values, tag_value);
        free(tag_value);
        return true;
    }
    return rsvc_illegal_short_option(opt, fail);
}

static bool check_options(string_list_t files, ops_t ops, rsvc_done_t fail) {
    if (!files->head) {
        rsvc_errorf(fail, __FILE__, __LINE__, "no input files");
        return false;
    } else if (cloak_mode(ops) < 0) {
        rsvc_errorf(fail, __FILE__, __LINE__, "no actions");
        return false;
    }
    return true;
}
