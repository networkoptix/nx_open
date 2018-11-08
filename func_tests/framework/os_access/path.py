import errno
import logging
import os
import stat
import sys
from abc import ABCMeta, abstractmethod

import pathlib2 as pathlib
import six

from framework.os_access import exceptions

_logger = logging.getLogger(__name__)


class FileSystemPath(pathlib.Path):
    __metaclass__ = ABCMeta

    def __new__(cls, *args, **kwargs):
        return cls._from_parts(args)

    def _init(self, template=None):
        if template is not None:
            assert isinstance(template, type(self))
            assert template._accessor is self._accessor
        self._closed = False

    @classmethod
    @abstractmethod
    def tmp(cls):
        return cls()

    def rmtree(self, ignore_errors=False, onerror=None):
        """Recursively delete a directory tree.

        If ignore_errors is set, errors are ignored; otherwise, if onerror
        is set, it is called to handle the error with arguments (func,
        path, exc_info) where func is os.listdir, os.remove, or os.rmdir;
        path is the argument to that function that caused it to fail; and
        exc_info is a tuple returned by sys.exc_info().  If ignore_errors
        is false and onerror is None, an exception is raised.

        """
        if ignore_errors:
            def onerror(*_args):
                pass
        elif onerror is None:
            def onerror(_failed_func, _path, exc_info):
                six.reraise(*exc_info)
        try:
            if self.is_symlink():
                # symlinks to directories are forbidden, see bug #1669
                raise OSError("Cannot call rmtree on a symbolic link")
        except OSError:
            onerror(self.is_symlink, self, sys.exc_info())
            # can't continue even if onerror hook returns
            return
        children = []
        try:
            children = list(self.iterdir())
        except OSError:
            onerror(self.iterdir, self, sys.exc_info())
        for child in children:
            if child.is_dir():
                child.rmtree(ignore_errors=ignore_errors, onerror=onerror)
            else:
                try:
                    child.unlink()
                except OSError:
                    onerror(self.unlink, child, sys.exc_info())
        try:
            self.rmdir()
        except OSError:
            onerror(self.rmdir, self, sys.exc_info())

    def yank(self, offset, max_length=None):
        with self.open('rb+') as f:
            f.seek(offset)
            return f.read(max_length)

    def patch(self, offset, data):
        with self.open('rb+') as f:
            f.seek(offset)
            return f.write(data)

    def size(self):
        path_stat = self.stat()
        if not stat.S_ISREG(path_stat.st_mode):
            raise exceptions.NotAFile(errno.EISDIR, "Stat: {}".format(path_stat))
        return path_stat.st_size

    def ensure_empty_dir(self):
        if self.exists():
            self.rmtree()
        self.mkdir(parents=True)

    def ensure_file_is_missing(self):
        if self.exists():
            self.unlink()

    @classmethod
    def tmp_file(cls, base_name):
        random_name = os.urandom(6).encode('hex')
        dir = cls.tmp()
        dir.mkdir(parents=True, exist_ok=True)
        return dir.joinpath(base_name.stem + '-' + random_name + base_name.suffix)

    def patch_string(self, regex, new_value, offset_cache_suffix):
        offset_path = self.with_name(self.name + offset_cache_suffix)

        def _try_cached_offset():
            _logger.debug("Read file with offset %s.", offset_path)
            try:
                offset_str = offset_path.read_text()
            except exceptions.DoesNotExist:
                _logger.debug("Cannot find file with offset %s.", offset_path)
                return
            offset = int(offset_str)
            _logger.debug("Check offset %d in %s.", offset, self)
            chunk = self.yank(offset, max_length=300)  # Length is arbitrary.
            match = regex.match(chunk)
            if match is None:
                return
            _logger.debug("Old value is %s.", match.group('value'))
            return match.start('value'), offset + match.start('value'), offset + match.end('padding')

        def _search_in_entire_file():
            contents = self.read_bytes()
            match = regex.search(contents)
            if match is None:
                raise RuntimeError("Cannot find a place matching {!r} in {!r}.".format(regex.pattern, self))
            offset_path.write_bytes(bytes(match.start()))
            return match.start('value'), match.start('value'), match.end('padding')

        old_value, begin, end = _try_cached_offset() or _search_in_entire_file()
        if old_value == new_value:
            _logger.info("New value is same as old %s.", new_value)
        if len(new_value) > end - begin:
            raise ValueError("New value %r is too long, max length is %d.", new_value, end - begin)
        patch = new_value.ljust(end - begin, b'\0')
        self.patch(begin, patch)
        return old_value

    def take_from(self, local_source_path):
        destination = self / local_source_path.name
        if not local_source_path.exists():
            raise exceptions.CannotDownload(
                "Local file {} doesn't exist.".format(local_source_path))
        if destination.exists():
            return destination
        copy_file(local_source_path, destination)
        return destination


def copy_file(source, destination, chunk_size_bytes=1024 * 1024):
    # type: (FileSystemPath, FileSystemPath, int) -> None
    _logger.info("Copy from %s to %s", source, destination)
    copied_bytes = 0
    while True:
        chunk = source.yank(copied_bytes, max_length=chunk_size_bytes)
        if copied_bytes == 0:
            destination.write_bytes(chunk)
        else:
            destination.patch(copied_bytes, chunk)
        copied_bytes += len(chunk)
        if len(chunk) < chunk_size_bytes:
            break
        assert len(chunk) == chunk_size_bytes
