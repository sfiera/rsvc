#!/bin/bash

set -o errexit
set -o nounset

diff -u - <(out/cur/rsvctest) <<EOF

  basename: ""
  dirname: "."
  ext: "(null)"
.
  basename: "."
  dirname: "."
  ext: "(null)"
..
  basename: ".."
  dirname: "."
  ext: "(null)"
/
  basename: "/"
  dirname: "/"
  ext: "(null)"
//
  basename: "/"
  dirname: "/"
  ext: "(null)"
./
  basename: "."
  dirname: "."
  ext: "(null)"
/.
  basename: "."
  dirname: "/"
  ext: "(null)"
/a/b/c
  basename: "c"
  dirname: "/a/b"
  ext: "(null)"
/a/b/c/
  basename: "c"
  dirname: "/a/b"
  ext: "(null)"
/a/b/c//
  basename: "c"
  dirname: "/a/b"
  ext: "(null)"
/a/b/c.d
  basename: "c.d"
  dirname: "/a/b"
  ext: "d"
/a/b/c.d/
  basename: "c.d"
  dirname: "/a/b"
  ext: "d"
/a/b/c.d//
  basename: "c.d"
  dirname: "/a/b"
  ext: "d"
a/b/c
  basename: "c"
  dirname: "a/b"
  ext: "(null)"
a/b/.c
  basename: ".c"
  dirname: "a/b"
  ext: "(null)"
a/b/.c/
  basename: ".c"
  dirname: "a/b"
  ext: "(null)"
a/b/.c//
  basename: ".c"
  dirname: "a/b"
  ext: "(null)"
i/am/an/excellent.mp3
  basename: "excellent.mp3"
  dirname: "i/am/an"
  ext: "mp3"
dot.here/no/extension
  basename: "extension"
  dirname: "dot.here/no"
  ext: "(null)"
dot.here/extension.mp3
  basename: "extension.mp3"
  dirname: "dot.here"
  ext: "mp3"
EOF
echo OK!
