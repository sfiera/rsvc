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

#include "unix.h"

#include <sys/param.h>
#include "common.h"

void print_results(const char* path) {
    char scratch[MAXPATHLEN];
    outf("%s\n", path);
    outf("  basename: \"%s\"\n", rsvc_basename(path, scratch));
    outf("  dirname: \"%s\"\n", rsvc_dirname(path, scratch));
    outf("  ext: \"%s\"\n", rsvc_ext(path, scratch));
}

int main(int argc, char** argv) {
    print_results("");
    print_results(".");
    print_results("..");
    print_results("/");
    print_results("//");
    print_results("./");
    print_results("/.");
    print_results("/a/b/c");
    print_results("/a/b/c/");
    print_results("/a/b/c//");
    print_results("/a/b/c.d");
    print_results("/a/b/c.d/");
    print_results("/a/b/c.d//");
    print_results("a/b/c");
    print_results("a/b/.c");
    print_results("a/b/.c/");
    print_results("a/b/.c//");
    print_results("i/am/an/excellent.mp3");
    print_results("dot.here/no/extension");
    print_results("dot.here/extension.mp3");
    return 0;
}
