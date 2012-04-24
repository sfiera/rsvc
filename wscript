# -*- mode: python -*-

APPNAME = "ripservice"
VERSION = "0.0.0"

WARNINGS = ["-Wall", "-Werror", "-Wno-sign-compare"]

def common(ctx):
    ctx.default_sdk = "10.7"
    ctx.default_compiler = "clang"
    ctx.load("compiler_c")
    ctx.load("core externals", tooldir="ext/waf-sfiera")
    ctx.external("discid flac libmusicbrainz mp4v2 vorbis")

def dist(dst):
    dst.algo = "zip"
    dst.excl = "**/.* **/*.zip **/*.pyc **/build"

def options(opt):
    common(opt)

def configure(cnf):
    common(cnf)
    cnf.env.append_value("FRAMEWORK_ripservice/system/diskarbitration", "DiskArbitration")
    cnf.env.append_value("FRAMEWORK_ripservice/system/audiotoolbox", "AudioToolbox")

def build(bld):
    common(bld)

    bld.program(
        target="ripservice/rsvc",
        features="universal",
        source="src/bin/rsvc.c",
        includes="include",
        cflags=WARNINGS,
        use="ripservice/librsvc",
    )

    bld.program(
        target="ripservice/cloak",
        features="universal",
        source="src/bin/cloak.c",
        includes="include",
        cflags=WARNINGS,
        use="ripservice/librsvc",
    )

    bld.stlib(
        target="ripservice/librsvc",
        features="universal",
        source=[
            "src/rsvc/cd.c",
            "src/rsvc/disc.c",
            "src/rsvc/common.c",
            "src/rsvc/flac.c",
            "src/rsvc/format.c",
            "src/rsvc/id3.c",
            "src/rsvc/mp4.c",
            "src/rsvc/musicbrainz.c",
            "src/rsvc/options.c",
            "src/rsvc/progress.c",
            "src/rsvc/tag.c",
            "src/rsvc/vorbis.c",
        ],
        includes="include",
        export_includes="include",
        cflags=WARNINGS,
        use=[
            "discid/libdiscid",
            "flac/libflac",
            "mp4v2/libmp4v2",
            "libmusicbrainz/libmusicbrainz-c",
            "vorbis/libvorbis",
        ],
    )

    bld.platform(
        target="ripservice/librsvc",
        platform="darwin",
        source="src/rsvc/core-audio.c",
        use=[
            "ripservice/system/audiotoolbox",
            "ripservice/system/diskarbitration",
        ],
    )

def doc(doc):
    import subprocess
    subprocess.call(["scripts/doc.sh"])
    subprocess.call("sphinx-build -b dirhtml . _build/html".split(), cwd="doc")
