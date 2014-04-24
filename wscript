# -*- mode: python -*-

from waflib.Utils import unversioned_sys_platform

APPNAME = "ripservice"
VERSION = "0.0.0"

WARNINGS = ["-Wall", "-Werror", "-Wno-sign-compare"]
CFLAGS = WARNINGS + ["-fblocks"]

def common(ctx):
    ctx.default_sdk = "10.8"
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
    cnf.env.append_value("FRAMEWORK_ripservice/system/cocoa", "Cocoa")

    if unversioned_sys_platform() != "darwin":
        cnf.check_cc(lib="BlocksRuntime", uselib_store="ripservice/system/blocks")
        cnf.check_cc(lib="dispatch", uselib_store="ripservice/system/dispatch")

def build(bld):
    common(bld)

    bld.program(
        target="ripservice/rsvc",
        features="universal",
        source="src/bin/rsvc.c",
        includes="include",
        cflags=CFLAGS,
        use="ripservice/librsvc",
    )

    bld.program(
        target="ripservice/cloak",
        features="universal",
        source="src/bin/cloak.c",
        includes="include",
        cflags=CFLAGS,
        use="ripservice/librsvc",
    )

    if unversioned_sys_platform() == "darwin":
        bld(
            rule="${SRC} %s %s ${TGT}" % (VERSION, bld.options.sdk),
            source="scripts/generate-info-plist.py",
            target="ripservice/Info.plist",
        )

        bld.program(
            target="ripservice/Rip Service",
            features="universal c cprogram",
            mac_app=True,
            mac_plist="ripservice/Info.plist",
            mac_resources=[
                "resources/AudioCDView.nib",
                "resources/MainMenu.nib",
            ],
            source=[
                "src/bin/rip-service.m",
                "src/app/app-delegate.m",
                "src/app/audio-cd-controller.m",
                "src/app/source-list.m",
                "src/app/source-list-cell.m",
            ],
            cflags=CFLAGS,
            use=[
                "ripservice/librsvc",
                "ripservice/system/cocoa",
            ],
        )

    bld.stlib(
        target="ripservice/librsvc",
        features="universal",
        source=[
            "src/rsvc/common.c",
            "src/rsvc/disc.c",
            "src/rsvc/encoding.c",
            "src/rsvc/flac.c",
            "src/rsvc/format.c",
            "src/rsvc/group.c",
            "src/rsvc/id3.c",
            "src/rsvc/mp4.c",
            "src/rsvc/musicbrainz.c",
            "src/rsvc/options.c",
            "src/rsvc/progress.c",
            "src/rsvc/tag.c",
            "src/rsvc/unix.c",
            "src/rsvc/vorbis.c",
        ],
        includes="include",
        export_includes="include",
        cflags=CFLAGS,
        defines="MB_VERSION=%d" % bld.env.mb_version,
        use=[
            "discid/libdiscid",
            "flac/libflac",
            "mp4v2/libmp4v2",
            "libmusicbrainz/libmusicbrainz",
            "vorbis/libvorbis",
        ],
    )

    bld.platform(
        target="ripservice/librsvc",
        platform="darwin",
        source=[
            "src/rsvc/cd_darwin.c",
            "src/rsvc/core-audio.c",
            "src/rsvc/disc_darwin.c",
            "src/rsvc/unix_darwin.c",
        ],
        use=[
            "ripservice/system/audiotoolbox",
            "ripservice/system/diskarbitration",
        ],
    )

    bld.platform(
        target="ripservice/librsvc",
        platform="linux",
        source=[
            "src/rsvc/cd_linux.c",
            "src/rsvc/unix_linux.c",
            "src/rsvc/disc_linux.c",
        ],
        use=[
            "ripservice/system/blocks",
            "ripservice/system/dispatch",
        ],
    )

def doc(doc):
    import subprocess
    subprocess.call(["scripts/doc.sh"])
    subprocess.call("sphinx-build -b dirhtml . _build/html".split(), cwd="doc")
