from abc import ABCMeta, abstractmethod

from pathlib2 import Path, PurePath


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
    def touch(self, mode, exist_ok):
        pass

    @abstractmethod
    def exists(self):
        return True

    @abstractmethod
    def unlink(self):
        pass

    @abstractmethod
    def iterdir(self):
        yield self.__class__()

    @abstractmethod
    def expanduser(self):
        return self.__class__()

    @abstractmethod
    def glob(self, pattern):
        yield self.__class__()

    @abstractmethod
    def rglob(self, pattern):
        yield self.__class__()

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
    def write_bytes(self, contents):
        pass

    @abstractmethod
    def read_text(self, encoding, errors):
        return u''

    @abstractmethod
    def write_text(self, data, encoding, errors):
        pass

    @abstractmethod
    def upload(self, local_source_path):  # type: (Path) -> None
        pass

    @abstractmethod
    def download(self, local_destination_path):  # type: (Path) -> None
        pass
