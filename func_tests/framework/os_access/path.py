from abc import ABCMeta, abstractmethod

from pathlib2 import PurePath


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

    @abstractmethod
    def mkdir(self, parents=False, exist_ok=True):
        pass

    @abstractmethod
    def rmtree(self, ignore_errors=False):
        pass

    @abstractmethod
    def read_bytes(self):
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


def copy_file(source, destination):  # type: (FileSystemPath, FileSystemPath) -> None
    destination.write_bytes(source.read_bytes())
