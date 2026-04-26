FROM alpine:3.23 AS builder

RUN apk add --no-cache \
    build-base \
    gn \
    ninja-build \
    pkgconf \
    python3 \
    clang

RUN apk add --no-cache \
    libdiscid-dev \
    flac-dev \
    lame-dev \
    libmad-dev \
    libogg-dev \
    opus-dev \
    opusfile-dev \
    taglib-dev \
    utfcpp \
    libvorbis-dev \
    libdispatch-dev \
    musl-fts-dev \
    eudev-dev \
    linux-headers \
    libc++-dev \
    zlib-dev \
    llvm-libunwind-dev

RUN rm /usr/include/sys/cdefs.h

WORKDIR /src

COPY . .

RUN ./configure && make 

FROM alpine:3.23

RUN apk add --no-cache \
    libdiscid \
    flac-libs \
    lame-libs \
    libmad \
    libogg \
    opus \
    opusfile \
    taglib \
    libvorbis \
    libdispatch \
    musl-fts \
    eudev-libs \
    linux-headers \
    libc++ \
    zlib \
    llvm-libunwind

COPY --from=builder /src/out/cur/cloak /src/out/cur/rsvc /bin

CMD ["/bin/rsvc"]
