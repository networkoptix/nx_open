#!/bin/env python


from __future__ import print_function
from glob import glob
from os.path import join
import logging
import os
import re
import subprocess

import archiver
import build_distribution_conf as conf


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


def archiveFiles(archiver, target_dir, source_dir, file_list):
    for file in file_list:
        logging.info("  Adding %s", os.path.basename(file))
        archiver.add(join(source_dir, file), join(target_dir, os.path.basename(file)))


def archiveByGlob(archiver, category, target_dir, pattern):
    logging.info("Finding %s going to %s from %s", category, target_dir, pattern)
    archiveFiles(archiver, target_dir, source_dir="", file_list=glob(pattern))


def main():
    isWindows = conf.CMAKE_SYSTEM_NAME == "Windows"

    bin_dir = "bin"
    lib_dir = bin_dir if isWindows else "lib"
    lib_globs = {
        "Linux": ["*.so*"],
        "Windows": ["*.dll", "*.pdb"],
        "Darwin": ["*.dylib"]
    }[conf.CMAKE_SYSTEM_NAME]

    with archiver.Archiver(conf.PACKAGE_FILE) as a:
        src_bin_dir = join(conf.BUILD_DIR, bin_dir)
        logging.info("Finding unit test executables going to %s at %s", bin_dir, src_bin_dir)
        archiveFiles(a, bin_dir, src_bin_dir, get_unit_tests_list())

        # NOTE: On Windows, mediaserver plugins go to "bin" to avoid PATH issues in unit tests.

        plugins_dir = join(bin_dir, "plugins")
        target_dir = bin_dir if isWindows else plugins_dir
        plugins_optional_dir = join(bin_dir, "plugins_optional")
        target_optional_dir = bin_dir if isWindows else plugins_optional_dir

        for lib_glob in lib_globs:
            archiveByGlob(a, "libraries", lib_dir,
                join(conf.BUILD_DIR, lib_dir, lib_glob))
            archiveByGlob(a, "mediaserver plugins", target_dir,
                join(conf.BUILD_DIR, plugins_dir, lib_glob))
            archiveByGlob(a, "mediaserver optional plugins", target_optional_dir,
                join(conf.BUILD_DIR, plugins_optional_dir, lib_glob))
            for plugin_group in ["sqldrivers"]:
                archiveByGlob(a, "Qt plugins from %s" % plugin_group, join(bin_dir, plugin_group),
                    join(conf.QT_DIR, "plugins", plugin_group, lib_glob))

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
                join(src_metadata_sdk_ut_dir, ut_lib_glob))
            archiveByGlob(a, "metadata_sdk nx_kit unit test libraries", metadata_sdk_dir,
                join(src_metadata_sdk_nx_kit_ut_dir, ut_lib_glob))

        archiveByGlob(a, "metadata_sdk nx_kit unit test executables", metadata_sdk_dir,
            join(src_metadata_sdk_nx_kit_ut_dir, ut_bin_glob))
        archiveByGlob(a, "metadata_sdk unit test executables", metadata_sdk_dir,
            join(src_metadata_sdk_ut_dir, ut_bin_glob))


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
