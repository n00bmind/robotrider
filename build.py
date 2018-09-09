#!/usr/bin/env python2

import os, sys, subprocess, atexit, random
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
default_config = config_win_develop
# default_config = config_win_debug



def begin_time():
    subprocess.call(['ctime', '-begin', 'rr.time'])

def end_time():
    # TODO Check this picks up failures etc.
    subprocess.call(['ctime', '-end', 'rr.time'])

    
if __name__ == '__main__':
    srcdir = 'src'
    bindir = 'bin'
    verbose = False

    atexit.register(end_time)
    begin_time()

    cwd = os.path.dirname(os.path.abspath(__file__))
    srcpath = os.path.join(cwd, srcdir)
    binpath = os.path.join(cwd, bindir)

    if not os.path.exists(binpath):
        os.mkdir(binpath)

    # TODO Process comand line
    # verbose = True

    # TODO Determine platform/config
    platform = default_platform
    config = default_config

    if platform.toolset == 'CL':
        # TODO Do the renaming of pdbs etc. so that reloading can work inside VS
        
        # Build game library
        args = [platform.compiler]
        args.extend(platform.common_compiler_flags)
        args.extend(config.compiler_flags)
        args.append(os.path.join(srcpath, 'robotrider.cpp'))
        args.append('-LD')
        args.append('/link')
        args.extend(platform.common_linker_flags)
        args.extend(config.linker_flags)
        # FIXME Generating a different PDB each time breaks debugging in VS when hot reloading
        args.append('/PDB:robotrider_{}.pdb'.format(random.randint(0, 100000)))

        if verbose:
            print('Building game library...')
            print(args)
        ret = subprocess.call(args, cwd=binpath)

        # Build platform executable
        args = [platform.compiler]
        args.extend(platform.common_compiler_flags)
        args.extend(config.compiler_flags)
        args.append(os.path.join(srcpath, 'win32_platform.cpp'))
        args.append('-Felauncher.exe')
        args.append('/link')
        args.extend(platform.common_linker_flags)
        args.extend(config.linker_flags)
        args.append('-subsystem:console,5.2')
        args.extend(platform.libs)

        if verbose:
            print('Building platform executable...')
            print(args)
        ret |= subprocess.call(args, cwd=binpath)

    else:
        sys.exit('Unsupported toolset')

    sys.exit(ret)
