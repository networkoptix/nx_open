#!/usr/bin/env python3

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import tarfile
import zipfile


class Archiver():
    def __init__(self, file_name, conf=None):
        self._file_name = file_name

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
        self._file.write(file_name, archive_name)

    def _add_to_tar(self, file_name, archive_name):
        self._file.add(file_name, archive_name)
