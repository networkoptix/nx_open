import logging
import os
from abc import ABCMeta, abstractmethod

from pathlib2 import PurePath

from framework.os_access.exceptions import NotADir

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


def copy_file(source, destination):  # type: (FileSystemPath, FileSystemPath) -> None
    _logger.info("Copy from %s to %s", source, destination)
    source.copy_to(destination)

def copy_file_using_read_and_write(source, destination):  # type: (FileSystemPath, FileSystemPath) -> None
    destination.write_bytes(source.read_bytes())
