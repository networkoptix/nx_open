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


def main():
    bin_dir = "bin"
    plugins_dir = join(bin_dir, "plugins")
    plugins_optional_dir = join(bin_dir, "plugins_optional")
    lib_dir = bin_dir if conf.CMAKE_SYSTEM_NAME == "Windows" else "lib"
    lib_glob = {
        "Linux": "*.so*",
        "Windows": "*.dll",
        "Darwin": "*.dylib"
    }[conf.CMAKE_SYSTEM_NAME]
    metadata_sdk_dir = "nx_metadata_sdk"
    metadata_sdk_ut_dir = join(metadata_sdk_dir, "metadata_sdk-build")
    metadata_sdk_nx_kit_ut_dir = join(metadata_sdk_ut_dir, join("nx_kit", "unit_tests"))
    metadata_ut_binary_glob = {
        "Linux": "*_ut",
        "Windows": "Debug\\*_ut.exe",
        "Darwin": "*_ut"
    }[conf.CMAKE_SYSTEM_NAME]
    metadata_ut_lib_glob = {
        "Linux": "*.so*",
        "Windows": "Debug\\*.dll",
        "Darwin": "*.dylib"
    }[conf.CMAKE_SYSTEM_NAME]

    with archiver.Archiver(conf.PACKAGE_FILE) as a:
        # Archive unit tests executables.
        src_bin_dir = join(conf.BUILD_DIR, bin_dir)
        for test in get_unit_tests_list():
            logging.info("Archiving unit test %s" % test)
            a.add(join(src_bin_dir, test), join(bin_dir, test))

        # Archive libraries.
        src_lib_dir = join(conf.BUILD_DIR, lib_dir)
        for lib in glob(join(src_lib_dir, lib_glob)):
            logging.info("Archiving library %s" % lib)
            a.add(lib, join(lib_dir, os.path.basename(lib)))

        # Archive mediaserver plugins.
        src_plugins_dir = join(conf.BUILD_DIR, plugins_dir)
        for plugin in glob(join(src_plugins_dir, lib_glob)):
            logging.info("Archiving mediaserver plugin %s" % plugin)
            a.add(plugin, join(plugins_dir, os.path.basename(plugin)))
        src_plugins_optional_dir = join(conf.BUILD_DIR, plugins_optional_dir)
        for plugin in glob(join(src_plugins_optional_dir, lib_glob)):
            logging.info("Archiving mediaserver optional plugin %s" % plugin)
            a.add(plugin, join(plugins_optional_dir, os.path.basename(plugin)))

        # Archive Qt plugins.
        for plugin_group in ["sqldrivers"]:
            for lib in glob(join(conf.QT_DIR, "plugins", plugin_group, lib_glob)):
                logging.info("Archiving Qt plugin %s" % lib)
                a.add(lib, join(bin_dir, plugin_group, os.path.basename(lib)))

        # Archive metadata_sdk unit tests.
        src_metadata_sdk_ut_dir = join(conf.BUILD_DIR, metadata_sdk_ut_dir)
        src_metadata_sdk_nx_kit_ut_dir = join(conf.BUILD_DIR, metadata_sdk_nx_kit_ut_dir)
        for lib in glob(join(src_metadata_sdk_ut_dir, metadata_ut_lib_glob)) \
                + glob(join(src_metadata_sdk_nx_kit_ut_dir, metadata_ut_lib_glob)):
            logging.info("Archiving metadata_sdk unit test library %s" % lib)
            a.add(lib, join(metadata_sdk_dir, os.path.basename(lib)))
        for test in glob(join(src_metadata_sdk_ut_dir, metadata_ut_binary_glob)) \
                + glob(join(src_metadata_sdk_nx_kit_ut_dir, metadata_ut_binary_glob)):
            logging.info("Archiving metadata_sdk unit test executable %s" % test)
            a.add(lib, join(metadata_sdk_dir, os.path.basename(test)))


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
