//
// This file is part of Rip Service.
//
// Copyright (C) 2016 Chris Pickel <sfiera@sfzmail.com>
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

static bool same_file(const char* x, const char* y) {
    rsvc_done_t ignore = ^(rsvc_error_t error){ (void)error; };
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

static const char* path_format_for(rsvc_format_t format, rsvc_tags_t tags, ops_t ops) {
    enum fpath_priority priority = FPATH_DEFAULT;
    const char* path = DEFAULT_PATH;
    for (format_path_list_node_t curr = ops->paths.head; curr; curr = curr->next) {
        if (curr->priority > priority) {
            if (curr->priority == FPATH_GROUP) {
                if (curr->group != format->format_group) {
                    continue;
                }
            } else if (curr->priority == FPATH_MEDIAKIND) {
                int n = 0;
                bool mediakind_matches = false;
                for (rsvc_tags_iter_t it = rsvc_tags_begin(tags); rsvc_next(it); ) {
                    if (strcmp(it->name, RSVC_MEDIAKIND) == 0) {
                        mediakind_matches = (++n == 1) && (strcmp(it->value, curr->mediakind) == 0);
                    }
                }
                if (!mediakind_matches) {
                    continue;
                }
            }
            priority = curr->priority;
            path = curr->path;
        }
    }
    return path;
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

bool cloak_move_file(const char* path, rsvc_format_t format, rsvc_tags_t tags, ops_t ops,
               rsvc_done_t fail) {
    bool success = false;
    char extension_scratch[MAXPATHLEN];
    char* extension = rsvc_ext(path, extension_scratch);
    char* parent = NULL;
    char* new_path = NULL;
    char* new_parent = NULL;
    if (!rsvc_tags_strf(tags, path_format_for(format, tags, ops), extension, &new_path, fail)) {
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
