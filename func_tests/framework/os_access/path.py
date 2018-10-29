import logging
import os
import stat
from abc import ABCMeta, abstractmethod

from pathlib2 import PurePath, PurePosixPath

from framework.os_access import exceptions
from framework.os_access.exceptions import DoesNotExist, NotADir

_logger = logging.getLogger(__name__)


class FileSystemPath(PurePath):
    __metaclass__ = ABCMeta

    def __init__(self, *parts):  # type: (*str) -> None
        """Merely for type hinting"""

    @classmethod
    @abstractmethod
    def home(cls):
        return cls()

    @classmethod
    @abstractmethod
    def tmp(cls):
        return cls()

    @abstractmethod
    def exists(self):
        return True

    @abstractmethod
    def unlink(self):
        pass

    @abstractmethod
    def expanduser(self):
        return self.__class__()

    @abstractmethod
    def glob(self, pattern):
        return [FileSystemPath()]

    def walk(self):
        children = self.glob('*')
        for child in children:
            try:
                for descendant in child.walk():
                    yield descendant
            except NotADir:
                yield child

    def mkdir(self, parents=False, exist_ok=False):
        try:
            self._mkdir_raw()
        except exceptions.AlreadyExists:
            if not exist_ok:
                raise
        except exceptions.BadParent:
            if not parents:
                raise
            self.parent.mkdir(parents=True, exist_ok=False)
            self.mkdir(parents=False, exist_ok=False)

    def _mkdir_raw(self):
        raise NotImplementedError("Either `mkdir` or `_mkdir_raw` must be implemented")

    def rmtree(self, ignore_errors=False):
        try:
            iter_entries = self.glob('*')
        except exceptions.DoesNotExist:
            if ignore_errors:
                pass
            else:
                raise
        except exceptions.NotADir:
            self.unlink()
        else:
            for entry in iter_entries:
                entry.rmtree()
            self.rmdir()

    def rmdir(self):
        raise NotImplementedError("Either `rmtree` or `rmdir` must be implemented")

    @abstractmethod
    def read_bytes(self, offset=0, max_length=None):
        return b''

    @abstractmethod
    def write_bytes(self, contents, offset=None):
        return 0

    def read_text(self, encoding='utf8', errors='strict'):
        data = self.read_bytes()
        text = data.decode(encoding=encoding, errors=errors)
        return text

    def write_text(self, text, encoding='utf8', errors='strict'):
        data = text.encode(encoding=encoding, errors=errors)
        bytes_written = self.write_bytes(data)
        assert bytes_written == len(data)
        return len(text)

    def stat(self):
        raise NotImplementedError("Either `size` or `stat` must be implemented.")

    def size(self):
        path_stat = self.stat()
        if not stat.S_ISREG(path_stat.st_mode):
            raise exceptions.NotAFile("Stat: {}".format(path_stat))
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
            except DoesNotExist:
                _logger.debug("Cannot find file with offset %s.", offset_path)
                return
            offset = int(offset_str)
            _logger.debug("Check offset %d in %s.", offset, self)
            chunk = self.read_bytes(offset=offset, max_length=300)  # Length is arbitrary.
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
        self.write_bytes(new_value.ljust(end - begin, b'\0'), offset=begin)
        return old_value

    def take_from(self, local_source_path):
        destination = self / local_source_path.name
        if not local_source_path.exists():
            raise exceptions.CannotDownload(
                "Local file {} doesn't exist.".format(local_source_path))
        if destination.exists():
            raise exceptions.AlreadyExists(
                "Cannot copy {!s} to {!s}".format(local_source_path, self),
                destination)
        copy_file(local_source_path, destination)
        return destination


class BasePosixPath(FileSystemPath, PurePosixPath):
    __metaclass__ = ABCMeta
    _tmp = '/tmp/func_tests'


def copy_file(source, destination, chunk_size_bytes=1024 * 1024):
    # type: (FileSystemPath, FileSystemPath, int) -> None
    _logger.info("Copy from %s to %s", source, destination)
    copied_bytes = 0
    while True:
        chunk = source.read_bytes(copied_bytes, max_length=chunk_size_bytes)
        if copied_bytes == 0:
            destination.write_bytes(chunk)
        else:
            destination.write_bytes(chunk, offset=copied_bytes)
        copied_bytes += len(chunk)
        if len(chunk) < chunk_size_bytes:
            break
        assert len(chunk) == chunk_size_bytes
