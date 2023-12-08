#!/bin/env python

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/


from __future__ import print_function
from glob import iglob
from fnmatch import fnmatch
from os.path import join
import itertools
from pathlib import Path
import logging
import os
import re
import subprocess
import sys
import yaml

import archiver
import build_distribution_conf as conf

from cmake_parser import parse_boolean

WINDOWS_QT_PLUGINS = [
    "imageformats",
    "platforms",
    "tls",
    "multimedia"
]


def load_unit_test_metainfo():
    path = Path(conf.METAINFO_FILE)
    if not path.exists():
        logging.error(f'Metainfo file {path} cannot be found')
        sys.exit(1)

    with open(path, 'r') as f:
        return yaml.load(f, Loader=yaml.SafeLoader)

    logging.error(f'Metainfo file {path} cannot be read')
    sys.exit(1)


def get_unit_tests_list(metainfo):
    test_regex = re.compile(r"\s+Test.+: (.+)")

    extension = {
        "Linux": "",
        "Windows": ".exe",
        "Darwin": ""
    }[conf.CMAKE_SYSTEM_NAME]

    limit_to_targets = None
    if conf.AFFECTED_TARGETS_LIST_FILE_NAME:
        with open(Path(conf.BUILD_DIR) / conf.AFFECTED_TARGETS_LIST_FILE_NAME) as f:
            limit_to_targets = f.read().splitlines()

    output = subprocess.check_output(
        [conf.CTEST_EXECUTABLE, "-N"],
        cwd=conf.BUILD_DIR)

    for line in output.splitlines():
        m = test_regex.match(line.decode("utf-8"))
        if m:
            test_target_name = m.group(1)
            if limit_to_targets is not None and test_target_name not in limit_to_targets:
                continue

            if test_target_name not in metainfo:
                logging.error(f"Metainfo for {test_target_name} not found!")
                sys.exit(1)
            yield test_target_name + extension


# TODO: Remove this function once Go unit test dependency issue is fixed.
def get_go_unit_tests_list(tests_directory: str, metainfo):
    extension = {
        "Linux": "",
        "Windows": ".exe",
        "Darwin": ""
    }[conf.CMAKE_SYSTEM_NAME]

    GO_UNIT_TEST_NAMES = [
        "nx_discovery_service_ut_discovery_ut",
        "nx_cs_licenser_ut_cssapi_ut",
        "nx_cs_licenser_ut_httpclientfactory_ut",
        "nx_cs_licenser_ut_licapi_ut",
        "nx_cs_licenser_ut_licensingpolicy_ut",
        "nx_cs_licenser_ut_model_ut",
        "nx_cs_licenser_ut_sysmapper_ut",
        "nx_go_libs_ut_nxconfig_ut",
        "nx_go_libs_ut_nxhttp_ut",
        "nx_go_libs_ut_nxrange_ut",
        "nx_go_libs_ut_nxtime_ut",
        "nx_go_libs_ut_nxoauth2_ut",
        "nx_service_authorizer_ut_authenticator_ut",
        "nx_service_authorizer_ut_functional_ut",
        "nx_service_authorizer_ut_jsondb_ut",
        "nx_bookmarks_service_ut_test_ut",
        "nx_chunk_log_service_ut_accesslog_ut",
        "nx_chunk_log_service_ut_api_ut",
        "nx_chunk_log_service_ut_manager_ut",
        "nx_chunk_log_service_ut_memmodel_ut",
        "nx_chunk_log_service_ut_service_ut",
        "nx_chunk_log_service_ut_sqlmodel_ut",
        "nx_chunk_log_service_ut_yugabytemodel_ut",
        "nx_chunk_log_service_ut_mongodbmodel_ut",
        "nx_storage_cleaner_ut_chunklogapi_ut",
        "nx_storage_cleaner_ut_cssapi_ut",
        "nx_storage_cleaner_ut_dao_ut",
        "nx_storage_cleaner_ut_httpclientfactory_ut",
        "nx_storage_cleaner_ut_licenserapi_ut",
        "nx_storage_cleaner_ut_model_ut",
        "nx_storage_cleaner_ut_storagecleaner_ut",
        "nx_cs_motion_db_ut_bitmask_ut",
        "nx_cs_motion_db_ut_model_runner_ut",
        "nx_cs_motion_db_ut_model_ut",
    ]
    for test_name in GO_UNIT_TEST_NAMES:
        test_full_name = f"{test_name}{extension}"
        if not (Path(tests_directory) / test_full_name).is_file():
            continue

        if test_name not in metainfo:
            logging.error(f"Metainfo for {test_name} not found!")
            sys.exit(1)
        yield test_full_name


def gatherFiles(source_dir, exclude):
    reject_name = lambda name: any([fnmatch(name, pattern) for pattern in exclude])
    def filter_list(names):
        to_remove = [n for n in names if reject_name(n)]
        for name in to_remove:
            # Calling remove() for subdirs prevents os.walk() from entering removed directory.
            names.remove(name)

    for dirname, subdirs, files in os.walk(source_dir):
        filter_list(subdirs)
        filter_list(files)

        # Add symlinks as files
        files += [name for name in subdirs if os.path.islink(join(dirname, name))]

        relative_root = os.path.relpath(dirname, source_dir)

        for name in files:
            yield os.path.normpath(join(relative_root, name))


def archiveMacOsFrameworks(archiver, target_dir, source_dir):
    for framework_dir in iglob(join(source_dir, "*.framework")):
        framework_dir_name = os.path.basename(framework_dir)
        framework, _ = os.path.splitext(framework_dir_name)
        library_path = os.path.normpath(join(framework_dir, framework))

        # Avoid adding frameworks without libraries
        if not os.path.exists(library_path):
            continue

        files = list(gatherFiles(framework_dir, exclude=["Headers", "*.prl"]))
        archiveFiles(archiver, join(target_dir, framework_dir_name), framework_dir, files,
            category=framework_dir_name)


def archiveFiles(archiver, target_dir, source_dir, file_list, category=None) -> int:
    if category:
        logging.info(f"Archiving {category}")

    archived_files_count = 0
    for f in file_list:
        source_file = join(source_dir, f)
        target_file = join(target_dir, f)
        logging.info(f"  Adding {target_file} ({source_file})")
        archiver.add(source_file, target_file)
        archived_files_count += 1
    return archived_files_count


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


def archiveSdkUnitTests(archiver, conf, src_bin_dir):
    ut_file_patterns = {
        "Linux": ["*.so*", "*_ut"],
        "Windows": ["*.dll", "*.pdb", "*_ut.exe"],
        "Darwin": ["*.dylib", "*_ut"]
    }[conf.CMAKE_SYSTEM_NAME]
    source_dir = Path(conf.METADATA_SDK_BUILD_DIR)
    file_list = list(
        itertools.chain.from_iterable(source_dir.glob(f"**/{p}") for p in ut_file_patterns))
    if not file_list:
        logging.info("No SDK unit test executables found")
        return

    target_dir = Path("nx_metadata_sdk")
    for f in file_list:
        # Analytics Plugin libraries must go to "plugins_optional/" to make analytics_plugin_ut.cfg
        # from the VMS build directory work.
        if "stub_analytics_plugin." in f.name:
            target_file = target_dir / "plugins_optional" / "stub_analytics_plugin" / f.name
        elif "_analytics_plugin." in f.name:
            target_file = target_dir / "plugins_optional" / f.name
        else:
            target_file = target_dir / f.name
        logging.info(f"  Adding {target_file} ({f})")
        archiver.add(f, target_file)

    # Copy Analytics Plugin unit test configuration file.
    archiver.add(
        join(src_bin_dir, "analytics_plugin_ut.cfg"),
        target_dir / "analytics_plugin_ut.cfg")


def main():
    isWindows = conf.CMAKE_SYSTEM_NAME == "Windows"
    isMac = conf.CMAKE_SYSTEM_NAME == "Darwin"
    withClient = parse_boolean(conf.WITH_CLIENT)
    withMediaServer = parse_boolean(conf.WITH_MEDIA_SERVER)

    bin_dir = "bin"
    lib_dir = bin_dir if isWindows else "lib"
    lib_globs = {
        "Linux": ["*.so*"],
        "Windows": ["*.dll", "*.pdb"],
        "Darwin": ["*.dylib"]
    }[conf.CMAKE_SYSTEM_NAME]
    metainfo = load_unit_test_metainfo()

    with archiver.Archiver(conf.PACKAGE_FILE, conf=conf) as a:
        src_bin_dir = join(conf.BUILD_DIR, bin_dir)

        logging.info("Archiving metadata file")
        a.add(conf.METAINFO_FILE, Path(conf.METAINFO_FILE).name)

        # Since the build system doesn't know on which files Go unit tests depend, always add
        # these unit tests. TODO: Fix the build system - add dependencies to Go unit test
        # generation.
        logging.info("Archiving Go unit tests")
        archiveFiles(a, bin_dir, src_bin_dir, get_go_unit_tests_list(src_bin_dir, metainfo))

        if not isMac:
            logging.info("Archiving standalone metadata_sdk unit tests")
            archiveSdkUnitTests(a, conf, src_bin_dir)

        logging.info("Archiving unit test executables")
        if not archiveFiles(a, bin_dir, src_bin_dir, get_unit_tests_list(metainfo)):
            logging.info("No unit test executables found")
            return

        # NOTE: On Windows, mediaserver plugins go to "bin" to avoid PATH issues in unit tests.

        plugins_dir = join(bin_dir, "plugins")
        target_plugins_dir = bin_dir if isWindows else plugins_dir
        plugins_optional_dir = join(bin_dir, "plugins_optional")
        translations_dir = join(bin_dir, "translations")
        target_plugins_optional_dir = bin_dir if isWindows else plugins_optional_dir

        for lib_glob in lib_globs:
            archiveByGlob(a, "libraries", lib_dir, join(conf.BUILD_DIR, lib_dir), lib_glob,
                recursive=True)
            archiveByGlob(a, "mediaserver plugins", target_plugins_dir,
                join(conf.BUILD_DIR, plugins_dir), lib_glob)
            archiveByGlob(a, "mediaserver optional plugins", target_plugins_optional_dir,
                join(conf.BUILD_DIR, plugins_optional_dir), lib_glob, recursive=True)
            for plugin_group in ["sqldrivers", "platforms", "webview", "tls", "multimedia"]:
                archiveByGlob(a, f"Qt plugins from {plugin_group}", join(bin_dir, plugin_group),
                    join(conf.QT_DIR, "plugins", plugin_group), lib_glob)

        # Archive client external resources.
        if withClient:
            archiveFiles(
                a,
                target_dir=bin_dir,
                source_dir=src_bin_dir,
                file_list=["client_external.dat", "client_core_external.dat", "bytedance_iconpark.dat"])

        # Archive VMS external resources.
        if withMediaServer and not parse_boolean(conf.IS_OPEN_SOURCE):
            archiveFiles(
                a,
                target_dir=bin_dir,
                source_dir=src_bin_dir,
                file_list=["external.dat"])

        # Archive translations
        archiveByGlob(a, "translations", translations_dir,
            join(src_bin_dir, "translations"), "*.dat")

        # Archive Qml components
        archiveByGlob(a, "Qt Qml components", bin_dir,
            join(conf.QT_DIR, "qml"), "*", recursive=True)

        archiveByGlob(a, "Config files", bin_dir, src_bin_dir, "*.cfg")

        if not parse_boolean(conf.IS_OPEN_SOURCE):
            archiveFiles(a, "", conf.BUILD_DIR, ["specific_features.txt"])

        # Archive Qt for Windows build (only dlls).
        if isWindows:
            dll_glob = "*.dll"
            for plugin_group in WINDOWS_QT_PLUGINS:
                archiveByGlob(a, f"Qt plugins from {plugin_group}", join(bin_dir, plugin_group),
                    join(conf.QT_DIR, "plugins", plugin_group), dll_glob)
            archiveByGlob(a, "Qt dlls", bin_dir, join(conf.QT_DIR, "bin"), dll_glob)

        if isMac:
            archiveMacOsFrameworks(a, lib_dir, join(conf.QT_DIR, "lib"))
        else:
            # On macOS all Qt WebEngine resources are a part of its framework,
            # but for other platforms they have to be added manually.
            dirname, pattern = \
                ("bin", "QtWebEngineProcess*.exe") if isWindows else ("libexec", "QtWebEngineProcess")
            archiveByGlob(a, "Qt WebEngine executable", bin_dir, join(conf.QT_DIR, dirname), pattern)
            archiveByGlob(a, "Qt WebEngine resources",
                join(bin_dir if isWindows else "", "resources"),
                join(conf.QT_DIR, "resources"),
                "*")
            archiveByGlob(a, "Qt WebEngine locales",
                join(translations_dir if isWindows else "translations", "qtwebengine_locales"),
                join(conf.QT_DIR, "translations", "qtwebengine_locales"),
                "*.pak")

        logging.info("Archiving misc files")

        # Add FFmpeg variant for ARM32 devices.
        if os.path.islink(join(conf.BUILD_DIR, lib_dir, "ffmpeg")):
            archiveFiles(a, lib_dir, join(conf.BUILD_DIR, lib_dir), ["ffmpeg"])

        archiveFiles(a, "", join(conf.BUILD_DIR, "distrib"), [conf.CONAN_REFS_FILE])


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
        logging.error(e, exc_info=sys.exc_info())
        exit(1)
