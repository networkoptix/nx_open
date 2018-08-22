import logging
import os
from abc import ABCMeta, abstractmethod

from pathlib2 import PurePath

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

    @abstractmethod
    def mkdir(self, parents=False, exist_ok=True):
        pass

    @abstractmethod
    def rmtree(self, ignore_errors=False):
        pass

    @abstractmethod
    def read_bytes(self, offset=0, max_length=None):
        return b''

    @abstractmethod
    def write_bytes(self, contents, offset=None):
        return 0

    @abstractmethod
    def read_text(self, encoding, errors):
        return u''

    @abstractmethod
    def write_text(self, data, encoding, errors):
        return 0

    def copy_to(self, destination):
        copy_file_using_read_and_write(self, destination)

    def copy_from(self, source):
        copy_file_using_read_and_write(source, self)

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
        return cls.tmp().joinpath(base_name.stem + '-' + random_name + base_name.suffix)

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


def copy_file(source, destination):  # type: (FileSystemPath, FileSystemPath) -> None
    _logger.info("Copy from %s to %s", source, destination)
    source.copy_to(destination)

def copy_file_using_read_and_write(source, destination):  # type: (FileSystemPath, FileSystemPath) -> None
    destination.write_bytes(source.read_bytes())
