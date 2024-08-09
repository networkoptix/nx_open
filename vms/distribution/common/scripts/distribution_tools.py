#!/usr/bin/env python3

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

from __future__ import print_function
import fnmatch
import subprocess
import shutil
import os
import zipfile
import platform
import sys
import argparse
import yaml
import shlex
from pathlib import Path
from collections import namedtuple
from typing import Generator


def is_windows():
    return platform.system() == 'Windows'


def is_macos():
    return platform.system() == 'Darwin'


def is_linux():
    return platform.system() == 'Linux'


def _dynamic_library_extension():
    if is_windows():
        return '.dll'
    elif is_macos():
        return '.dylib'
    else:
        return '.so'


def dynamic_library_filename(name, extension=None, version=None):
    if is_windows():
        return f'{name}{extension or ".dll"}'
    elif is_linux():
        return (
                f'lib{name}'
                + (extension or '.so')
                + (f'.{version}' if version else '')
        )
    elif is_macos():
        return (
                f'lib{name}'
                + (f'.{version}' if version else '')
                + (extension or '.dylib')
        )


def executable_filename(name):
    return f'{name}.exe' if is_windows() else name


def print_command(command):
    print(f'>> {subprocess.list2cmdline(command)}')


def execute_command(command, verbose=False, working_directory=None):
    if verbose:
        print_command(command)
    try:
        current_directory = os.getcwd()
        if working_directory:
            os.chdir(working_directory)
        subprocess.check_output(command, stderr=subprocess.STDOUT)
        if working_directory:
            os.chdir(current_directory)
    except Exception as e:
        if not verbose:
            print_command(command)
        print(f'Error: {e.output}', file=sys.stderr)
        raise


def rename(folder, old_name, new_name):
    full_old_name = os.path.join(folder, old_name)
    full_new_name = os.path.join(folder, new_name)
    if os.path.exists(full_new_name):
        os.unlink(full_new_name)
    if os.path.exists(full_old_name):
        shutil.copy2(full_old_name, full_new_name)


def find_files_by_template(dir, template):
    for file in fnmatch.filter(os.listdir(dir), template):
        yield os.path.join(dir, file)


def find_all_files(dir):
    for root, dirs, files in os.walk(dir):
        for file in files:
            yield os.path.join(root, file)


def ffmpeg_files(source_dir):
    templates = (
        (
            f'{template}*{_dynamic_library_extension()}' if is_windows()
            else f'{dynamic_library_filename(template, None, "*")}'
        ) for template in (
        'av*',
        'swscale',
        'swresample'
    )
    )

    for template in templates:
        for file in find_files_by_template(source_dir, template):
            yield file


def openssl_files(source_dir):
    if is_windows():
        templates = ['libcrypto-1_1-x64', 'libssl-1_1-x64']
    else:
        templates = ['crypto*', 'ssl*']

    for template in templates:
        files = find_files_by_template(source_dir, dynamic_library_filename(template, None, '*'))
        for file in files:
            yield file


def hidapi_files(source_dir):
    template = 'hidapi-hidraw' if is_linux() else 'hidapi'
    yield os.path.join(source_dir, dynamic_library_filename(template))


def openal_files(source_dir):
    yield os.path.join(source_dir, f'OpenAL32{_dynamic_library_extension()}')


def quazip_files_to(source_dir):
    yield os.path.join(source_dir, f'quazip{_dynamic_library_extension()}')


def icu_files(source_dir):
    if is_windows():
        templates = ['icudt74', 'icuin74', 'icuuc74']
    else:
        templates = ['icudata', 'icui18n', 'icuuc']

    for template in templates:
        yield from find_files_by_template(
            source_dir, dynamic_library_filename(template, None, '74'))


def shared_libraries(directory: str) -> Generator[str, None, None]:
    for file in Path(directory).glob(f"*{_dynamic_library_extension()}"):
        yield file


def qt_libraries(dir, libs, extension=None):
    for lib in libs:
        for file in find_files_by_template(
                dir,
                dynamic_library_filename(f'Qt6{lib}', extension=extension) + '*'
        ):
            yield file


def qt_plugins_files(qt_plugins_dir, libs):
    for lib in libs:
        yield os.path.join(qt_plugins_dir, lib)


def nx_files(source_dir, libs):
    for lib in libs:
        yield os.path.join(source_dir, dynamic_library_filename(lib))


def versioned_libraries(source_dir, libs):
    for lib in libs:
        lib_file_names = Path(source_dir).glob(dynamic_library_filename(lib, version="*"))
        for lib_file_name in lib_file_names:
            yield str(lib_file_name)


def zip_files(files, from_directory, zip_file, to_directory='.', mandatory=True):
    with zipfile.ZipFile(zip_file, "w", zipfile.ZIP_DEFLATED) as zip:
        for filename in files:
            source_file = os.path.join(from_directory, filename)
            if mandatory or os.path.isfile(source_file):
                zip.write(source_file, os.path.join(to_directory, filename))


def zip_files_to(zip, files, rel_path, target_path='.'):
    for file in files:
        target_filename = os.path.join(target_path, os.path.relpath(file, rel_path))
        zip.write(file, target_filename)


def zip_all_files(zip, directory):
    zip_files_to(zip, find_all_files(directory), directory)


def shell_like_parse(infile):
    result = {}

    for kv in [line.strip().split('=', 2) for line in infile]:
        if len(kv) != 2:
            continue

        key, value = kv[0], kv[1]

        # Parse the value, shlex.split() should take care of comments and whitespaces.

        if value.startswith('('):  # Array.
            parsed_elements = shlex.split(value[1:], comments=True)

            # Last parsed element should be either '<arg...>)' or just ')'.
            if parsed_elements[-1] == ')':
                value = parsed_elements[:-1]
            elif parsed_elements[-1].endswith(')'):
                value[-1] = parsed_elements[-1][:-1]

        else:  # Value.
            value = shlex.split(value, comments=True)
            # shlex.split() always returns a list, but we need a single value here.
            if value:
                value = value[0]

        result[key] = value

    return result


def parse_generation_scripts_arguments():
    Arguments = namedtuple('Arguments', ['config', 'output_file', 'targets'])

    parser = argparse.ArgumentParser()
    parser.add_argument('--config', help="Config files via semicolon", required=True)
    parser.add_argument('--output_file', help="Output file", required=True)
    parser.add_argument('--targets', help="Target files via semicolon")
    args = parser.parse_args()

    config = {}
    config_files = args.config.split(';')

    for config_file in config_files:
        with open(config_file, 'r') as f:
            if config_file.endswith('.conf'):
                config_part = shell_like_parse(f)
            else:
                config_part = yaml.load(f, Loader=yaml.SafeLoader)
            config.update(config_part)

    return Arguments(config=config, output_file=args.output_file, targets=args.targets.split(';'))
