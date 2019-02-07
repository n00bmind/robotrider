# -*- coding: utf-8 -*-
# The MIT License

# Copyright (c) 2017 Oscar Peñas Pariente <oscarpp80@gmail.com>

# Permission is hereby granted, free of charge, to any person obtaining a copy of
# this software and associated documentation files (the "Software"), to deal in
# the Software without restriction, including without limitation the rights to
# use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
# the Software, and to permit persons to whom the Software is furnished to do so,
# subject to the following conditions:

# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
# IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#!/usr/bin/env python2

from __future__ import print_function

import os, sys, subprocess, atexit, random, argparse
from collections import namedtuple


Platform = namedtuple('Platform', ['name', 'compiler', 'toolset', 'common_compiler_flags', 'libs', 'common_linker_flags'])

platform_win = Platform(
        name                  = 'win',
        compiler              = 'cl.exe',
        toolset               = 'CL',
        common_compiler_flags = [
            '-MTd', '-nologo', '-FC', '-W4', '-WX', '-Oi', '-GR-', '-EHa-',
            '-D_HAS_EXCEPTIONS=0', '-D_CRT_SECURE_NO_WARNINGS',
            '-wd4201', '-wd4100', '-wd4189', '-wd4101', '-wd4505', '-wd4312', '-wd4200'
            ],
        libs                  = ['user32.lib', 'gdi32.lib', 'winmm.lib', 'ole32.lib', 'opengl32.lib', 'shlwapi.lib'],
        common_linker_flags   = ['/opt:ref', '/incremental:no']
)

platform_win_clang = platform_win._replace(
        name                  = 'win_clang',
        compiler              = 'clang-cl.exe',
        common_compiler_flags = platform_win.common_compiler_flags + [
            '-fdiagnostics-absolute-paths',
            '-Wno-missing-braces',
            '-Wno-unused-variable',
            '-Wno-unused-function',
            '-Wno-missing-field-initializers'
        ],
)



Config = namedtuple('Configuration', ['name', 'platform', 'cmdline_opts', 'compiler_flags', 'linker_flags'])

config_win_debug = Config(
        name           = 'Debug',
        platform       = platform_win,
        cmdline_opts   = ['d', 'dbg', 'debug'],
        compiler_flags = ['-DDEBUG=1', '-Z7', '-Od'],
        linker_flags   = ['/debug:full']                # Required for debugging after hot reloading
)
# This config is extremely confusing and we need to clarify this:
# The choice of whether we have certain "development" features (like an editor mode) is a platform thing
# totally unrelated to build mode, and so it should be reflected in the platform - game interface.
# (for example, by adding keyboard/mouse info for the editor, which a PC would provide by a phone wouldn't)
# This build only makes sense as a faster non-release build to use during development when Debug is just
# too slow, and hence it should be renamed to something like 'Profile' or similar (also, it remains to
# be seen how useful all the debug information is in this context)
config_win_develop = Config(
        name           = 'Develop',
        platform       = platform_win,
        cmdline_opts   = ['dev', 'develop'],
        compiler_flags = ['-DDEVELOP=1', '-Z7', '-O2'],
        linker_flags   = ['/debug:full']
)
config_win_release = Config(
        name           = 'Release',
        platform       = platform_win,
        cmdline_opts   = ['r', 'rel', 'release'],
        compiler_flags = ['-DRELEASE=1', '-O2'],
        linker_flags   = ['/debug:full']
)


default_platform = platform_win
# default_config = config_win_debug
default_config = config_win_develop
# default_config = config_win_release


class colors:
    GRAY = '\033[1;30m'
    END = '\033[0m'
# print '\033[1;30mGray like Ghost\033[1;m'
# print '\033[1;31mRed like Radish\033[1;m'
# print '\033[1;32mGreen like Grass\033[1;m'
# print '\033[1;33mYellow like Yolk\033[1;m'
# print '\033[1;34mBlue like Blood\033[1;m'
# print '\033[1;35mMagenta like Mimosa\033[1;m'
# print '\033[1;36mCyan like Caribbean\033[1;m'
# print '\033[1;37mWhite like Whipped Cream\033[1;m'
# print '\033[1;38mCrimson like Chianti\033[1;m'
# print '\033[1;41mHighlighted Red like Radish\033[1;m'
# print '\033[1;42mHighlighted Green like Grass\033[1;m'
# print '\033[1;43mHighlighted Brown like Bear\033[1;m'
# print '\033[1;44mHighlighted Blue like Blood\033[1;m'
# print '\033[1;45mHighlighted Magenta like Mimosa\033[1;m'
# print '\033[1;46mHighlighted Cyan like Caribbean\033[1;m'
# print '\033[1;47mHighlighted Gray like Ghost\033[1;m'
# print '\033[1;48mHighlighted Crimson like Chianti\033[1;m'

def print_color(output, color_string):
    print(color_string, end='')
    print(output, end='')
    print(colors.END)


def begin_time():
    subprocess.call(['ctime', '-begin', 'rr.time'])

def end_time():
    # TODO Check this picks up failures etc.
    subprocess.call(['ctime', '-end', 'rr.time'])

    
if __name__ == '__main__':
    srcdir = 'src'
    bindir = 'bin'

    parser = argparse.ArgumentParser()
    config_group = parser.add_mutually_exclusive_group()
    config_group.add_argument('-d', '--debug', help='Create Debug build', action='store_true')
    config_group.add_argument('-r', '--release', help='Create Release build', action='store_true')
    parser.add_argument('-v', '--verbose', help='Increase verbosity', action='store_true')
    in_args = parser.parse_args()

    atexit.register(end_time)
    begin_time()

    cwd = os.path.dirname(os.path.abspath(__file__))
    srcpath = os.path.join(cwd, srcdir)
    binpath = os.path.join(cwd, bindir)

    if not os.path.exists(binpath):
        os.mkdir(binpath)
    else:
        for file in os.listdir(binpath):
            filelow = file.lower()
            if filelow.startswith('robotrider') and filelow.endswith('.pdb'):
                try:
                    pdbpath = os.path.join(binpath, file)
                    os.remove(pdbpath)
                except WindowsError:
                    # print('Couldn\'t remove {} (probably in use)'.format(pdbpath))
                    pass

    # TODO Determine platform/config
    platform = default_platform
    config = default_config
    if in_args.debug:
        config = config_win_debug
    elif in_args.release:
        config = config_win_release

    if platform.toolset == 'CL':
        # TODO Do the renaming of pdbs etc. so that reloading can work inside VS (?)
        
        # Build game library
        out_args = [platform.compiler]
        out_args.extend(platform.common_compiler_flags)
        out_args.extend(config.compiler_flags)
        out_args.append(os.path.join(srcpath, 'robotrider.cpp'))
        out_args.append('-LD')
        out_args.append('/link')
        out_args.extend(platform.common_linker_flags)
        out_args.extend(config.linker_flags)
        # FIXME Generating a different PDB each time breaks debugging in VS when hot reloading (?)
        out_args.append('/PDB:robotrider_{}.pdb'.format(random.randint(0, 100000)))

        if in_args.verbose:
            print('\nBuilding game library...')
            print_color(out_args, colors.GRAY)
        # Create a lock file so the platform doesn't try to load the dll too soon
        lockfilepath = os.path.join(binpath, 'dll.lock')
        os.close(os.open(lockfilepath, os.O_CREAT))
        ret = subprocess.call(out_args, cwd=binpath)
        os.remove(lockfilepath)

        # Build platform executable
        out_args = [platform.compiler]
        out_args.extend(platform.common_compiler_flags)
        out_args.extend(config.compiler_flags)
        out_args.append(os.path.join(srcpath, 'win32_platform.cpp'))
        out_args.append('-Felauncher.exe')
        out_args.append('/link')
        out_args.extend(platform.common_linker_flags)
        out_args.extend(config.linker_flags)
        out_args.append('-subsystem:console,5.2')
        out_args.extend(platform.libs)

        if in_args.verbose:
            print('\nBuilding platform executable...')
            print_color(out_args, colors.GRAY)
        ret |= subprocess.call(out_args, cwd=binpath)

        # Build test suite
        out_args = [platform.compiler]
        out_args.extend(platform.common_compiler_flags)
        out_args.extend(config.compiler_flags)
        out_args.append(os.path.join(srcpath, 'testsuite.cpp'))
        out_args.append('/link')
        out_args.extend(platform.common_linker_flags)
        out_args.extend(config.linker_flags)
        out_args.append('-subsystem:console,5.2')
        # out_args.extend(platform.libs)

        if in_args.verbose:
            print('\nBuilding test suite...')
            print_color(out_args, colors.GRAY)
        ret |= subprocess.call(out_args, cwd=binpath)

    else:
        sys.exit('Unsupported toolset')

    sys.exit(ret)
