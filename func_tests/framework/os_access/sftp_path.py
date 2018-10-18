import errno
import fnmatch
import logging
import stat
from abc import ABCMeta, abstractproperty
from functools import wraps

import paramiko
from paramiko import SFTPFile

from framework.os_access import exceptions
from framework.os_access.path import BasePosixPath

_logger = logging.getLogger(__name__)


class _UnknownSftpError(Exception):
    def __init__(self, e):
        super(_UnknownSftpError, self).__init__('_UnknownSftpError [{}]: {}'.format(e.errno, e))


def _reraise_by_errno(errno_to_exception):
    def decorator(func):
        @wraps(func)
        def decorated(self, *args, **kwargs):
            try:
                return func(self, *args, **kwargs)
            except IOError as e:
                if e.errno not in errno_to_exception:
                    raise
                raise errno_to_exception[e.errno]

        return decorated

    return decorator


class SftpPath(BasePosixPath):
    __metaclass__ = ABCMeta

    _client = abstractproperty()  # type: paramiko.SFTPClient
    _home = abstractproperty()

    @classmethod
    def specific_cls(cls, client, home):
        # TODO: Delay `%TEMP%` and `%USERPROFILE%` expansion until accessed and use them.
        class SpecificSftpPath(cls):
            _client = client
            _home = home

        return SpecificSftpPath

    @classmethod
    def home(cls):
        return cls(cls._home)

    @classmethod
    def tmp(cls):
        return cls(cls._tmp)

    @_reraise_by_errno({2: exceptions.BadPath("Path doesn't exist or is a file")})
    def glob(self, pattern):
        return [self / name for name in fnmatch.filter(self._client.listdir(str(self)), pattern)]

    def exists(self):
        try:
            self._client.stat(str(self))
        except IOError as e:
            if e.errno != errno.ENOENT:
                raise
            return False
        else:
            return True

    @_reraise_by_errno({
        None: exceptions.BadPath("Probably a dir."),
        })
    def unlink(self):
        self._client.remove(str(self))

    def expanduser(self):
        raise NotImplementedError()

    @_reraise_by_errno({
        errno.ENOENT: exceptions.BadParent(),
        None: exceptions.AlreadyExists("Already exists and may be dir or file"),
        })
    def _mkdir_raw(self):
        self._client.mkdir(str(self))

    def rmtree(self, ignore_errors=False):
        try:
            entries = self._client.listdir_attr(path=str(self))
        except IOError as e:
            if e.errno != errno.ENOENT:
                raise
            if ignore_errors:
                return
            raise exceptions.DoesNotExist()
        for entry in entries:
            if stat.S_ISREG(entry.st_mode):
                self.joinpath(entry.filename).unlink()
            if stat.S_ISDIR(entry.st_mode):
                self.joinpath(entry.filename).rmtree(ignore_errors=False)
        self.rmdir()

    def rmdir(self):
        self._client.rmdir(str(self))

    @_reraise_by_errno({
        None: exceptions.NotAFile(),
        errno.ENOENT: exceptions.DoesNotExist(),
        })
    def read_bytes(self, offset=None, max_length=None):
        """Read paramiko.file.BufferedFile#read docstring and code. It does exactly what's needed:
        returns either `max_length` bytes or bytes until the end of file.
        """
        if offset is not None:
            with self.open('rb+') as f:
                f.seek(offset)
                return f.read(max_length)
        else:
            with self.open('rb') as f:
                return f.read(max_length)

    def write_bytes(self, contents, offset=None):
        """paramiko.file.BufferedFile#write writes all it's fed. File is not buffered (it's set
        when opening the file) -- no flushing is required.
        """
        if offset is not None:
            with self.open('rb+') as f:
                f.seek(offset)
                f.write(contents)
        else:
            with self.open('wb') as f:
                f.write(contents)
        return len(contents)

    @_reraise_by_errno({
        2: exceptions.DoesNotExist(),
        })
    def stat(self):
        s = self._client.stat(str(self))
        return s

    def open(self, mode):  # type: (str) -> SFTPFile
        """Open file in stream mode. Seeking is not possible. If opened for writing, file is
        truncated. Can be used not only for regular files."""
        try:
            f = self._client.open(str(self), mode)
        except IOError as e:
            if 'w' in mode:
                if e.errno == errno.ENOENT:
                    raise exceptions.BadParent()
                if e.errno is None:
                    raise exceptions.NotAFile()
                raise
            if 'r' in mode:
                if e.errno == errno.ENOENT:
                    raise exceptions.DoesNotExist()
                if e.errno is None:
                    raise exceptions.BadParent()
                raise
            raise
        return f
