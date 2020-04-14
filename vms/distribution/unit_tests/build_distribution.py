#!/bin/env python


from __future__ import print_function
from glob import iglob
from fnmatch import fnmatch
from os.path import join
import logging
import os
import re
import subprocess

import archiver
import build_distribution_conf as conf

WINDOWS_QT_PLUGINS = [
    "audio",
    "imageformats",
    "mediaservice",
    "platforms"
]


def get_unit_tests_list():
    test_regex = re.compile(r"\s+Test.+: (.+)")

    extension = {
        "Linux": "",
        "Windows": ".exe",
        "Darwin": ""
    }[conf.CMAKE_SYSTEM_NAME]

    output = subprocess.check_output(
        [conf.CTEST_EXECUTABLE, "-N"],
        cwd=conf.BUILD_DIR)

    for line in output.splitlines():
        m = test_regex.match(line.decode("utf-8"))
        if m:
            yield m.group(1) + extension


def archiveMacOsFrameworks(archiver, target_dir, source_dir):
    for framework_dir in iglob(join(source_dir, "*.framework")):
        framework, _ = os.path.splitext(os.path.basename(framework_dir))
        library_name = os.path.normpath(join(framework_dir, framework))
        if not os.path.exists(library_name):
            continue

        logging.info("  Adding %s.framework", framework)

        real_name = os.path.realpath(library_name)
        relative_name = os.path.relpath(real_name, source_dir)
        archiver.add(real_name, join(target_dir, relative_name))


def archiveFiles(archiver, target_dir, source_dir, file_list, category=None):
    if category:
        logging.info(f"Archiving {category}")

    for f in file_list:
        source_file = join(source_dir, f)
        target_file = join(target_dir, f)
        logging.info(f"  Adding {target_file} ({source_file})")
        archiver.add(source_file, target_file)


def archiveByGlob(archiver, category, target_dir, source_dir, pattern, recursive=False):
    if recursive:
        files = []
        for root, _, dir_files in os.walk(source_dir):
            relative_root = os.path.relpath(root, source_dir)
            for f in dir_files:
                if fnmatch(f, pattern):
                    files.append(os.path.normpath(join(relative_root, f)))
    else:
        full_pattern = join(source_dir, pattern)
        files = [os.path.relpath(f, source_dir) for f in iglob(full_pattern, recursive=recursive)]

    archiveFiles(archiver, target_dir, source_dir, files, category=category)


def main():
    isWindows = conf.CMAKE_SYSTEM_NAME == "Windows"
    isMac = conf.CMAKE_SYSTEM_NAME == "Darwin"

    bin_dir = "bin"
    lib_dir = bin_dir if isWindows else "lib"
    lib_globs = {
        "Linux": ["*.so*"],
        "Windows": ["*.dll", "*.pdb"],
        "Darwin": ["*.dylib"]
    }[conf.CMAKE_SYSTEM_NAME]

    with archiver.Archiver(conf.PACKAGE_FILE) as a:
        src_bin_dir = join(conf.BUILD_DIR, bin_dir)
        logging.info("Archiving unit test executables")
        archiveFiles(a, bin_dir, src_bin_dir, get_unit_tests_list())

        # NOTE: On Windows, mediaserver plugins go to "bin" to avoid PATH issues in unit tests.

        plugins_dir = join(bin_dir, "plugins")
        target_plugins_dir = bin_dir if isWindows else plugins_dir
        plugins_optional_dir = join(bin_dir, "plugins_optional")
        target_plugins_optional_dir = bin_dir if isWindows else plugins_optional_dir

        for lib_glob in lib_globs:
            archiveByGlob(a, "libraries", lib_dir, join(conf.BUILD_DIR, lib_dir), lib_glob,
                recursive=True)
            archiveByGlob(a, "mediaserver plugins", target_plugins_dir,
                join(conf.BUILD_DIR, plugins_dir), lib_glob)
            archiveByGlob(a, "mediaserver optional plugins", target_plugins_optional_dir,
                join(conf.BUILD_DIR, plugins_optional_dir), lib_glob)
            for plugin_group in ["sqldrivers"]:
                archiveByGlob(a, f"Qt plugins from {plugin_group}", join(bin_dir, plugin_group),
                    join(conf.QT_DIR, "plugins", plugin_group), lib_glob)

        # Archive Qt for Windows build (only dlls).
        if isWindows:
            dll_glob = "*.dll"
            for plugin_group in WINDOWS_QT_PLUGINS:
                archiveByGlob(a, f"Qt plugins from {plugins_group}", join(bin_dir, plugin_group),
                    join(conf.QT_DIR, "plugins", plugin_group), dll_glob)
            archiveByGlob(a, "Qt dlls", bin_dir, join(conf.QT_DIR, "bin"), dll_glob)

        if isMac:
            archiveMacOsFrameworks(a, lib_dir, join(conf.QT_DIR, "lib"))

        # Archive metadata_sdk unit tests.
        ut_bin_glob = "Debug\\*_ut.exe" if isWindows else "*_ut"
        ut_lib_globs = {
            "Linux": ["*.so*"],
            "Windows": ["Debug\\*.dll", "Debug\\*.pdb"],
            "Darwin": ["*.dylib"]
        }[conf.CMAKE_SYSTEM_NAME]
        metadata_sdk_dir = "nx_metadata_sdk"
        metadata_sdk_ut_dir = join(metadata_sdk_dir, "metadata_sdk-build")
        metadata_sdk_nx_kit_ut_dir = join(metadata_sdk_ut_dir, join("nx_kit", "unit_tests"))
        src_metadata_sdk_ut_dir = join(conf.BUILD_DIR, metadata_sdk_ut_dir)
        src_metadata_sdk_nx_kit_ut_dir = join(conf.BUILD_DIR, metadata_sdk_nx_kit_ut_dir)
        for ut_lib_glob in ut_lib_globs:
            archiveByGlob(a, "metadata_sdk unit test libraries", metadata_sdk_dir,
                src_metadata_sdk_ut_dir, ut_lib_glob)
            archiveByGlob(a, "metadata_sdk nx_kit unit test libraries", metadata_sdk_dir,
                src_metadata_sdk_nx_kit_ut_dir, ut_lib_glob)

        archiveByGlob(a, "metadata_sdk nx_kit unit test executables", metadata_sdk_dir,
            src_metadata_sdk_nx_kit_ut_dir, ut_bin_glob)
        archiveByGlob(a, "metadata_sdk unit test executables", metadata_sdk_dir,
            src_metadata_sdk_ut_dir, ut_bin_glob)


if __name__ == "__main__":
    try:
        log_dir = os.path.dirname(conf.LOG_FILE)
        if not os.path.isdir(log_dir):
            os.makedirs(log_dir)
        print("  See the log in %s" % conf.LOG_FILE)
        logging.basicConfig(
            level=logging.INFO,
            format="%(message)s",
            filename=conf.LOG_FILE,
            filemode="w")
        main()
    except Exception as e:
        logging.error(e)
        exit(1)
