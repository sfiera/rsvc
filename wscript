# -*- mode: python -*-

APPNAME = "ripservice"
VERSION = "0.0.0"

WARNINGS = ["-Wall", "-Werror", "-Wno-sign-compare"]

def common(ctx):
    ctx.default_sdk = "10.6"
    ctx.default_compiler = "clang"
    ctx.load("compiler_c")
    ctx.load("core externals", tooldir="ext/waf-sfiera")
    ctx.external("discid flac vorbis")

def dist(dst):
    dst.algo = "zip"
    dst.excl = "**/.* **/*.zip **/*.pyc **/build"

def options(opt):
    common(opt)

def configure(cnf):
    common(cnf)

def build(bld):
    common(bld)

    bld.program(
        target="ripservice/ripservice",
        features="universal",
        source=[
            "src/bin/ripservice.c",
            "src/rsvc/cd.c",
            "src/rsvc/disc.c",
            "src/rsvc/common.c",
            "src/rsvc/flac.c",
            "src/rsvc/tag.c",
            "src/rsvc/vorbis.c",
        ],
        includes="include",
        cflags=WARNINGS,
        use=[
            "discid/libdiscid",
            "flac/libflac",
            "vorbis/libvorbis",
        ],
    )

def doc(doc):
    import subprocess
    subprocess.call(["scripts/doc.sh"])
    subprocess.call("sphinx-build -b dirhtml . _build/html".split(), cwd="doc")
