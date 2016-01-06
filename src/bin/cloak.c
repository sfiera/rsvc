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

static void print_tags(rsvc_tags_t tags);
static void print_images(rsvc_tags_t tags);
static void tag_files(string_list_node_t node, ops_t ops, rsvc_done_t done);
static void apply_ops(rsvc_tags_t tags, const char* path, rsvc_format_t format, ops_t ops,
                      rsvc_done_t done);

static void register_all_formats() {
    rsvc_audio_formats_register();
    rsvc_image_formats_register();
}

static void cloak_main(int argc, char* const* argv) {
    const char* progname = strdup(basename(argv[0]));
    rsvc_done_t fail = ^(rsvc_error_t error){
        if (error) {
            errf("%s: %s (%s:%d)\n",
                      progname, error->message, error->file, error->lineno);
            exit(1);
        }
        exit(0);
    };

    struct ops ops = {};
    struct string_list files = {};

    register_all_formats();
    if (cloak_options(argc, argv, &ops, &files, fail)) {
        tag_files(files.head, &ops, fail);
        dispatch_main();
    }
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

    rsvc_format_t format;
    if (!rsvc_format_detect(path, fd, &format, done)) {
        return;
    } else if (!format->open_tags) {
        rsvc_errorf(done, __FILE__, __LINE__, "%s: can't tag %s file", path, format->name);
        return;
    }

    rsvc_tags_t tags;
    if (!format->open_tags(path, cloak_mode(ops), &tags, done)) {
        return;
    }

    apply_ops(tags, path, format, ops, ^(rsvc_error_t error){
        if (error) {
            rsvc_tags_destroy(tags);
            done(error);
            return;
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
        rsvc_tags_destroy(tags);
        done(NULL);
    });
}

static void print_tags(rsvc_tags_t tags) {
    rsvc_tags_each(tags, ^(const char* name, const char* value,
                           rsvc_stop_t stop){
        outf("%s=", name);
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
    });
}

static void print_images(rsvc_tags_t tags) {
    rsvc_tags_image_each(tags, ^(
                rsvc_format_t format, const uint8_t* data, size_t size, rsvc_stop_t stop){
        struct rsvc_image_info info;
        rsvc_done_t fail = ^(rsvc_error_t err){
            outf("%zu-byte %s image\n", size, format->name);
        };
        if (format->image_info("embedded image", data, size, &info, fail)) {
            outf("%zu×%zu %s image\n", info.width, info.height, format->name);
        }
    });
}

static void tag_files(string_list_node_t node, ops_t ops, rsvc_done_t done) {
    if (!node) {
        done(NULL);
        return;
    }
    tag_file(node->value, ops, ^(rsvc_error_t error){
        if (error) {
            done(error);
            return;
        }
        if (ops->list_mode && node->next) {
            outf("\n");
        }
        tag_files(node->next, ops, done);
    });
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
    outf("%s", copy);
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

    outf("rename: ");
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
        outf("{");
        for (size_t i = 0; i < src_equal; ++i) {
            print_advance_segment(&srcseg, &nsrcseg);
        }
        outf(" => ");
        for (size_t i = 0; i < dst_equal; ++i) {
            print_advance_segment(&dstseg, &ndstseg);
        }
        outf("}");
    }
    outf("\n");

    free(srcseg_save);
    free(dstseg_save);
}

bool same_file(const char* x, const char* y) {
    void (^ignore)(rsvc_error_t) = ^(rsvc_error_t error){};
    bool same = false;
    int a_fd, b_fd;
    if (rsvc_open(x, O_RDONLY, 0644, &a_fd, ignore)
            && rsvc_open(y, O_RDONLY, 0644, &b_fd, ignore)) {
        struct stat a_st, b_st;
        if ((fstat(a_fd, &a_st) >= 0) && (fstat(b_fd, &b_st) >= 0)) {
            same = (a_st.st_dev == b_st.st_dev)
                && (a_st.st_ino == b_st.st_ino);
        }
        close(a_fd);
        close(b_fd);
    }
    return same;
}

const char* path_format_for(rsvc_format_t format, ops_t ops) {
    enum fpath_priority priority = FPATH_DEFAULT;
    const char* path = DEFAULT_PATH;
    for (format_path_list_node_t curr = ops->paths.head; curr; curr = curr->next) {
        if (curr->priority > priority) {
            if (curr->priority == FPATH_GROUP) {
                if (curr->group != format->format_group) {
                    continue;
                }
            }
            priority = curr->priority;
            path = curr->path;
        }
    }
    return path;
}

bool move_file(const char* path, rsvc_format_t format, rsvc_tags_t tags, ops_t ops,
               rsvc_done_t fail) {
    bool success = false;
    char extension_scratch[MAXPATHLEN];
    char* extension = rsvc_ext(path, extension_scratch);
    char* parent = NULL;
    char* new_path = NULL;
    char* new_parent = NULL;
    if (!rsvc_tags_strf(tags, path_format_for(format, ops), extension, &new_path, fail)) {
        goto end;
    }

    if (ops->dry_run) {
        if (!same_file(path, new_path)) {
            print_rename(path, new_path);
        }
    } else {
        char parent[MAXPATHLEN], new_parent[MAXPATHLEN];
        rsvc_dirname(path, parent);
        rsvc_dirname(new_path, new_parent);
        if (!(rsvc_makedirs(new_parent, 0755, fail)
              && rsvc_mv(path, new_path, fail))) {
            goto end;
        }
        rsvc_trimdirs(parent);
    }

    success = true;
end:
    if (parent) free(parent);
    if (new_path) free(new_path);
    if (new_parent) free(new_parent);
    return success;
}

static void apply_ops(rsvc_tags_t tags, const char* path, rsvc_format_t format, ops_t ops,
                      rsvc_done_t done) {
    if (ops->remove_all_tags) {
        if (!rsvc_tags_clear(tags, done)) {
            return;
        }
    } else {
        for (string_list_node_t curr = ops->remove_tags.head; curr; curr = curr->next) {
            if (!rsvc_tags_remove(tags, curr->value, done)) {
                return;
            }
        }
    }

    if (ops->remove_all_images) {
        if (!rsvc_tags_image_clear(tags, done)) {
            return;
        }
    } else {
        for (remove_image_list_node_t curr = ops->remove_images.head; curr; curr = curr->next) {
            if (!rsvc_tags_image_remove(tags, curr->index, done)) {
                return;
            }
        }
    }

    for (string_list_node_t curr = ops->add_tag_names.head; curr; curr = curr->next) {
        const char* name = curr->value;
        const char* value = curr->value;
        if (!rsvc_tags_add(tags, done, name, value)) {
            return;
        }
    }
    for (write_image_list_node_t curr = ops->write_images.head; curr; curr = curr->next) {
        const int index = curr->index;
        rsvc_logf(1, "writing image %d to %s", index, curr->temp_path);
        if ((index < 0) || (rsvc_tags_image_size(tags) <= index)) {
            rsvc_errorf(done, __FILE__, __LINE__, "bad index: %d", index);
            return;
        }

        __block int i = 0;
        __block bool success = false;
        rsvc_tags_image_each(tags, ^(
                    rsvc_format_t format, const uint8_t* data, size_t size, rsvc_stop_t stop){
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
                    strcat(image_path, format->extension);
                }
                success =
                    rsvc_write(curr->path, curr->fd, data, size, NULL, NULL, done)
                    && rsvc_rename(curr->temp_path, image_path, done);
                stop();
            }
        });
        if (!success) {
            return;
        }
    }

    for (add_image_list_node_t curr = ops->add_images.head; curr; curr = curr->next) {
        const char* path = curr->path;
        int fd = curr->fd;
        rsvc_format_t format = curr->format;
        uint8_t* data;
        size_t size;
        if (!rsvc_mmap(path, fd, &data, &size, done)) {
            return;
        }
        struct rsvc_image_info image_info;
        if (!format->image_info(path, data, size, &image_info, done)) {
            munmap(data, size);
            return;
        }
        rsvc_logf(
                2, "adding %s image from %s (%zux%zu, depth %zu, %zu-color)",
                format->name, path, image_info.width, image_info.height, image_info.depth,
                image_info.palette_size);
        if (!rsvc_tags_image_add(tags, format, data, size, done)) {
            munmap(data, size);
            return;
        }
        munmap(data, size);
    }

    if (!((ops->move_mode ? move_file(path, format, tags, ops, done) : true)
          && (ops->auto_mode ? rsvc_apply_musicbrainz_tags(tags, done) : true)
          && (!ops->dry_run ? rsvc_tags_save(tags, done) : true))) {
        return;
    }
    done(NULL);
}

int main(int argc, char* const* argv) {
    cloak_main(argc, argv);
    return EX_OK;
}
