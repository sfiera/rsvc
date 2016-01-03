#!/usr/bin/env


from __future__ import division, print_function, unicode_literals
import contextlib
import os
import shlex
import subprocess
import sys

def platform():
    if sys.platform == "darwin":
        return "mac"
    for platform in ["linux", "win"]:
        if sys.platform.startswith(platform):
            return platform
    return "unknown"


def tint(text, color):
    if not (color and os.isatty(1)):
        return text
    color = {
        "red": 31,
        "green": 32,
        "yellow": 33,
        "blue": 34,
    }[color]
    return "\033[1;%dm%s\033[0m" % (color, text)


@contextlib.contextmanager
def step(message):
    sys.stdout.write(message + "...")
    sys.stdout.flush()
    padding = ((37 - len(message)) * " ")
    def msg(failure, color=None):
        print(padding + tint(failure, color))
        msg.called = True
    msg.called = False
    yield msg
    if not msg.called:
        print(padding + tint("ok", "green"))


def check_bin(cmdline, input=None):
    with step("checking for %s" % cmdline[0]) as msg:
        stdin = None
        if input is not None:
            stdin = subprocess.PIPE
        try:
            p = subprocess.Popen(cmdline, stdin=stdin, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            p.communicate(input)
            if p.returncode == 0:
                return True
        except OSError:
            pass
        msg("missing", color="red")
        return False


def has_lib(name, includes, defines, cflags, headers, library_dirs, libraries):
    """Compile a basic C++11, libc++ binary."""
    src = []
    src.extend("#include <%s>" % i for i in headers)
    src.append("int main() { return 1; }")

    flags = []
    flags.extend("-I%s" % x for x in includes)
    flags.extend("-D%s" % x for x in defines)
    flags.extend(cflags)
    flags.extend("-L%s" % x for x in library_dirs)
    flags.extend("-l%s" % x for x in libraries)

    cmdline = ["clang"] + flags + ["-x", "c", "-", "-o", "/dev/null"]

    p = subprocess.Popen(
            cmdline, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    p.communicate("\n".join(src))
    if p.returncode == 0:
        return True
    return False


def check_pkg(lib):
    with step("checking for %s" % lib) as msg:
        try:
            p = subprocess.Popen(["pkg-config", lib])
            p.communicate()
            if p.returncode == 0:
                return True
        except OSError:
            pass
        msg("missing", color="red")
        return False


def pkg_settings(lib):
    try:
        p = subprocess.Popen(
                ["pkg-config", "--cflags", lib], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        cflags = p.communicate()[0]
        if p.returncode != 0:
            return None
        cflags = shlex.split(cflags)
        p = subprocess.Popen(
                ["pkg-config", "--libs", lib], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        ldflags = p.communicate()[0]
        if p.returncode != 0:
            return None
        ldflags = shlex.split(ldflags)
    except OSError:
        return None
    includes = [flag[2:] for flag in cflags if flag[:2] == "-I"]
    defines = [flag[2:] for flag in cflags if flag[:2] == "-D"]
    cflags = [flag for flag in cflags if flag[:2] not in ["-I", "-D"]]
    library_dirs = [flag[2:] for flag in ldflags if flag[:2] == "-L"]
    libraries = [flag[2:] for flag in ldflags if flag[:2] == "-l"]
    ldflags = [flag for flag in ldflags if flag[:2] not in ["-L", "-l"]]
    return includes, defines, cflags, library_dirs, libraries


def check_clang():
    """Compile a basic C++11, libc++ binary."""
    return check_bin(
            "clang -x c++ -std=c++11 -stdlib=libc++ - -o /dev/null".split(),
            input="int main() { return 1; }")


def check_gyp():
    """Run gyp --help (it doesn't have --version)."""
    return check_bin("gyp --help".split())


def check_ninja():
    """Run ninja --version.

    GNU Ninja doesn't have a --version flag, so this check should fail
    if the user accidentally installed ninja instead of ninja-build.
    """
    return check_bin("ninja --version".split())


def check_pkg_config():
    """Run pkg-config --version."""
    return check_bin("pkg-config --version".split())


def makedirs(path):
    try:
        os.makedirs(path)
    except OSError as e:
        if e.errno != 17:
            raise


def linkf(dst, src):
    try:
        os.unlink(src)
    except OSError as e:
        pass
    os.symlink(dst, src)
