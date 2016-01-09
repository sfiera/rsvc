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

#include "cloak.h"

static bool tag_files(string_list_t list, ops_t ops, rsvc_done_t fail);
static bool apply_ops(rsvc_tags_t tags, const char* path, rsvc_format_t format, ops_t ops,
                      rsvc_done_t fail);

static void cloak_main(int argc, char* const* argv) {
    const char* progname = strdup(basename(argv[0]));
    rsvc_done_t fail = ^(rsvc_error_t error){
        errf("%s: %s (%s:%d)\n", progname, error->message, error->file, error->lineno);
        exit(1);
    };

    struct ops ops = {};
    struct string_list files = {};
    rsvc_audio_formats_register();
    rsvc_image_formats_register();
    if (cloak_options(argc, argv, &ops, &files, fail) &&
        tag_files(&files, &ops, fail)) {
        exit(0);
    }
}

static bool check_taggable(rsvc_format_t format, rsvc_done_t fail) {
    if (!format->open_tags) {
        rsvc_errorf(fail, __FILE__, __LINE__, "can't tag %s file", format->name);
        return false;
    }
    return true;
}

static bool tag_file(const char* path, ops_t ops, rsvc_done_t fail) {
    bool result = false;
    int fd;
    if (rsvc_open(path, O_RDONLY, 0644, &fd, fail)) {
        // From here on, prepend the file name to the error message.
        fail = ^(rsvc_error_t error) { rsvc_prefix_error(path, error, fail); };
        rsvc_format_t format;
        rsvc_tags_t tags;
        if (rsvc_format_detect(path, fd, &format, fail) &&
            check_taggable(format, fail) &&
            format->open_tags(path, cloak_mode(ops), &tags, fail)) {
            if (apply_ops(tags, path, format, ops, fail)) {
                result = true;
            }
            rsvc_tags_destroy(tags);
        }
        close(fd);
    }
    return result;
}

static void print_tags(rsvc_tags_t tags) {
    for (rsvc_tags_iter_t it = rsvc_tags_begin(tags); rsvc_next(it); ) {
        outf("%s=", it->name);
        const char* value = it->value;
        while (*value) {
            size_t size = strcspn(value, "\\\r\n");
            fwrite(value, sizeof(char), size, stdout);
            value += size;
            size = strspn(value, "\\");
            if (size && strcspn(value + size, "\r\n")) {
                fwrite(value, sizeof(char), size, stdout);
            }
            fwrite(value, sizeof(char), size, stdout);
            value += size;
            if (strstr(value, "\r\n") == value) {
                outf("\\\n");
                value += 2;
            } else if (strspn(value, "\r\n")) {
                outf("\\\n");
                value += 1;
            }
        }
        outf("\n");
    }
}

static void print_images(rsvc_tags_t tags) {
    for (rsvc_tags_image_iter_t it = rsvc_tags_image_begin(tags); rsvc_next(it); ) {
        struct rsvc_image_info info;
        rsvc_done_t ignore = ^(rsvc_error_t error){ (void)error; };
        if (it->format->image_info("embedded image", it->data, it->size, &info, ignore)) {
            outf("%zu×%zu %s image\n", info.width, info.height, it->format->name);
        } else {
            outf("%zu-byte %s image\n", it->size, it->format->name);
        }
    }
}

static bool tag_files(string_list_t list, ops_t ops, rsvc_done_t fail) {
    for (string_list_node_t curr = list->head; curr; curr = curr->next) {
        if (!tag_file(curr->value, ops, fail)) {
            return false;
        } else if (ops->list_mode && curr->next) {
            outf("\n");
        }
    }
    return true;
}

static bool apply_ops(rsvc_tags_t tags, const char* path, rsvc_format_t format, ops_t ops,
                      rsvc_done_t fail) {
    if (ops->remove_all_tags) {
        if (!rsvc_tags_clear(tags, fail)) {
            return false;
        }
    } else {
        for (string_list_node_t curr = ops->remove_tags.head; curr; curr = curr->next) {
            if (!rsvc_tags_remove(tags, curr->value, fail)) {
                return false;
            }
        }
    }

    if (ops->remove_all_images) {
        if (!rsvc_tags_image_clear(tags, fail)) {
            return false;
        }
    } else {
        for (remove_image_list_node_t curr = ops->remove_images.head; curr; curr = curr->next) {
            if (!rsvc_tags_image_remove(tags, curr->index, fail)) {
                return false;
            }
        }
    }

    for (string_list_node_t name = ops->add_tag_names.head, value = ops->add_tag_values.head;
         name && value; name = name->next, value = value->next) {
        if (!rsvc_tags_add(tags, fail, name->value, value->value)) {
            return false;
        }
    }

    for (write_image_list_node_t curr = ops->write_images.head; curr; curr = curr->next) {
        const int index = curr->index;
        rsvc_logf(1, "writing image %d to %s", index, curr->temp_path);
        if ((index < 0) || (rsvc_tags_image_size(tags) <= index)) {
            rsvc_errorf(fail, __FILE__, __LINE__, "bad index: %d", index);
            return false;
        }

        int i = 0;
        bool success = false;
        for (rsvc_tags_image_iter_t it = rsvc_tags_image_begin(tags); rsvc_next(it); ) {
            if (index == i++) {
                char image_path[MAXPATHLEN + 10];  // space for extra dot and extension.
                strcpy(image_path, curr->path);
                if (!image_path[0]) {
                    char parent[MAXPATHLEN];
                    rsvc_dirname(path, parent);
                    strcat(image_path, parent);
                    strcat(image_path, "/cover");
                }
                char scratch[MAXPATHLEN];
                if (!rsvc_ext(image_path, scratch)) {
                    strcat(image_path, ".");
                    strcat(image_path, it->format->extension);
                }
                success =
                    rsvc_write(curr->path, curr->fd, it->data, it->size, NULL, NULL, fail)
                    && rsvc_rename(curr->temp_path, image_path, fail);
                rsvc_break(it);
                break;
            }
        }
        if (!success) {
            return false;
        }
    }

    for (add_image_list_node_t curr = ops->add_images.head; curr; curr = curr->next) {
        const char* path = curr->path;
        int fd = curr->fd;
        rsvc_format_t format = curr->format;
        uint8_t* data;
        size_t size;
        if (!rsvc_mmap(path, fd, &data, &size, fail)) {
            return false;
        }
        struct rsvc_image_info image_info;
        if (!format->image_info(path, data, size, &image_info, fail)) {
            munmap(data, size);
            return false;
        }
        rsvc_logf(
                2, "adding %s image from %s (%zux%zu, depth %zu, %zu-color)",
                format->name, path, image_info.width, image_info.height, image_info.depth,
                image_info.palette_size);
        if (!rsvc_tags_image_add(tags, format, data, size, fail)) {
            munmap(data, size);
            return false;
        }
        munmap(data, size);
    }

    if (!(ops->move_mode ? cloak_move_file(path, format, tags, ops, fail) : true) ||
        !(ops->auto_mode ? rsvc_apply_musicbrainz_tags(tags, fail) : true) ||
        !(ops->dry_run ? true : rsvc_tags_save(tags, fail))) {
        return false;
    }

    if (ops->list_mode == LIST_MODE_LONG) {
        outf("%s:\n", path);
    }
    if (ops->list_tags) {
        print_tags(tags);
    }
    if (ops->list_images) {
        print_images(tags);
    }

    return true;
}

int main(int argc, char* const* argv) {
    cloak_main(argc, argv);
    return EX_OK;
}
