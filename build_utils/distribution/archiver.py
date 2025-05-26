#!/usr/bin/env python3

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import tarfile
import fnmatch
import zipfile


class Archiver():
    def __init__(self, file_name, conf=None):
        self._file_name = file_name
        self._ignored_file_pattern_list = []
        self._important_file_pattern_list = []

        if file_name.endswith(".zip"):
            compresslevel = None if conf is None else conf.ZIP_COMPRESSION_LEVEL
            self._file = zipfile.ZipFile(
                file_name,
                mode="w",
                compression=zipfile.ZIP_DEFLATED,
                compresslevel=compresslevel)
            self.add = self._add_to_zip
        elif file_name.endswith(".tar.gz"):
            compresslevel = None if conf is None else conf.GZIP_COMPRESSION_LEVEL
            self._file = tarfile.open(
                file_name,
                mode="w:gz",
                encoding="utf-8",
                compresslevel=compresslevel)
            self.add = self._add_to_tar
        else:
            raise Exception("Unsupported archive format: %s" % file_name)

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self._file.close()

    def _add_to_zip(self, file_name, archive_name):
        filter_enabled = True

        for pattern in self._important_file_pattern_list:
            if fnmatch.fnmatch(archive_name, pattern):
                filter_enabled = False

        if filter_enabled:
            for pattern in self._ignored_file_pattern_list:
                if fnmatch.fnmatch(archive_name, pattern):
                    return

        self._file.write(file_name, archive_name)

    def _add_to_tar(self, file_name, archive_name):
        filter_enabled = True

        for pattern in self._important_file_pattern_list:
            if pattern[0] == "/":
                glob_pattern = pattern[1:]
            else:
                glob_pattern = '*' + pattern

            if fnmatch.fnmatch(archive_name, glob_pattern):
                filter_enabled = False

        if filter_enabled:
            for pattern in self._ignored_file_pattern_list:
                if pattern[0] == "/":
                    glob_pattern = pattern[1:]
                else:
                    glob_pattern = '*' + pattern

                if fnmatch.fnmatch(archive_name, glob_pattern):
                    return

        self._file.add(file_name, archive_name)

    def load_ignore_file(self, ignore_file):
        # Ignore files have the same format as .gitignore

        with open(ignore_file, "r") as f:
            lines = [ line.strip() for line in f.readlines() ]
            lines = filter(lambda line: line and not line.startswith("#"), lines)
            for line in lines:
                if line.startswith("!"):
                    self._important_file_pattern_list.append(line[1:])
                else:
                    self._ignored_file_pattern_list.append(line)
