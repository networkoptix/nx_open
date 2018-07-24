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
    lib_dir = bin_dir if conf.CMAKE_SYSTEM_NAME == "Windows" else "lib"
    lib_glob = {
        "Linux": "*.so*",
        "Windows": "*.dll",
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

        # Archive Qt plugins.
        for plugin_group in ["sqldrivers"]:
            for lib in glob(join(conf.QT_DIR, "plugins", plugin_group, lib_glob)):
                logging.info("Archiving Qt plugin %s" % lib)
                a.add(lib, join(bin_dir, plugin_group, os.path.basename(lib)))


if __name__ == "__main__":
    try:
        log_dir = os.path.dirname(conf.LOG_FILE)
        if not os.path.isdir(log_dir):
            os.makedirs(log_dir)

        print("See the log in %s" % conf.LOG_FILE)
        logging.basicConfig(level=logging.INFO, format="%(message)s", filename=conf.LOG_FILE)
        main()
    except Exception as e:
        logging.error(e)
