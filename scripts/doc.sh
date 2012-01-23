#!/bin/bash

set -o errexit

mkdir -p doc
find doc -name '*.h.rst' | xargs rm

cd include
find . -name '*.h' | while read IN; do
    OUT=../doc/$IN.rst
    mkdir -p $(dirname ../doc/$IN)
    cat $IN \
        | sed -E 's,( *)(.*/// ?(.*)|.*),\1\3,' \
        | sed 's/  *$//' \
        | sed '/./,$!d' \
        | cat -s \
        >$OUT
    touch -r$IN $OUT
done
