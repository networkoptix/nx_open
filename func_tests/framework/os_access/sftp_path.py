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
from framework.os_access.ssh_shell import SSH

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
                raise errno_to_exception[e.errno](e.errno, e.strerror)

        return decorated

    return decorator


class SftpPath(BasePosixPath):
    __metaclass__ = ABCMeta

    @abstractproperty
    def _client(self):  # type: () -> paramiko.SFTPClient
        pass

    @classmethod
    def specific_cls(cls, ssh):  # type: (SSH) -> ...
        class SpecificSftpPath(cls):
            @classmethod
            def home(cls):
                return cls(ssh.home_dir(ssh.current_user_name()))

            @property
            def _client(self):
                return ssh._sftp()

        return SpecificSftpPath

    @classmethod
    def tmp(cls):
        return cls(cls._tmp)

    @_reraise_by_errno({2: exceptions.BadPath})
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
        errno.ENOENT: exceptions.DoesNotExist,
        None: exceptions.BadPath,
        })
    def unlink(self):
        self._client.remove(str(self))

    def expanduser(self):
        raise NotImplementedError()

    @_reraise_by_errno({
        errno.ENOENT: exceptions.BadParent,
        None: exceptions.AlreadyExists,
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
            raise exceptions.DoesNotExist(errno.ENOENT, e.strerror)
        for entry in entries:
            if stat.S_ISDIR(entry.st_mode):
                self.joinpath(entry.filename).rmtree(ignore_errors=False)
            else:
                self.joinpath(entry.filename).unlink()
        self.rmdir()

    @_reraise_by_errno({
        errno.ENOENT: exceptions.DoesNotExist,
        None: exceptions.NotEmpty,
        })
    def rmdir(self):
        self._client.rmdir(str(self))

    @_reraise_by_errno({
        None: exceptions.NotAFile,
        errno.ENOENT: exceptions.DoesNotExist,
        })
    def read_bytes(self):
        """Read paramiko.file.BufferedFile#read docstring and code. It does exactly what's needed:
        returns either `max_length` bytes or bytes until the end of file.
        """
        with self.open('rb') as f:
            return f.read()

    @_reraise_by_errno({
        None: exceptions.NotAFile,
        errno.ENOENT: exceptions.DoesNotExist,
        })
    def yank(self, offset, max_length=None):
        """Read paramiko.file.BufferedFile#read docstring and code. It does exactly what's needed:
        returns either `max_length` bytes or bytes until the end of file.
        """
        with self.open('rb+') as f:
            f.seek(offset)
            return f.read(max_length)

    def write_bytes(self, contents):
        """paramiko.file.BufferedFile#write writes all it's fed. File is not buffered (it's set
        when opening the file) -- no flushing is required.
        """
        with self.open('wb') as f:
            f.write(contents)
        return len(contents)

    def patch(self, offset, contents):
        """paramiko.file.BufferedFile#write writes all it's fed. File is not buffered (it's set
        when opening the file) -- no flushing is required.
        """
        with self.open('rb+') as f:
            f.seek(offset)
            f.write(contents)
        return len(contents)

    @_reraise_by_errno({
        2: exceptions.DoesNotExist,
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
                    raise exceptions.BadParent(e.errno, e.strerror)
                if e.errno is None:
                    raise exceptions.NotAFile(errno.EISDIR, e.strerror)
                raise
            if 'r' in mode:
                if e.errno == errno.ENOENT:
                    raise exceptions.DoesNotExist(e.errno, e.strerror)
                if e.errno is None:
                    raise exceptions.BadParent(errno.EISDIR, e.strerror)
                raise
            raise
        return f

    @_reraise_by_errno({
        errno.ENOENT: exceptions.BadParent,
        None: exceptions.AlreadyExists,
        })
    def symlink_to(self, target, target_is_directory=False):
        if not isinstance(target, type(self)):
            raise ValueError(
                "Symlink can only point to same OS but link is {} and target is {}".format(
                    self, target))
        self._client.symlink(str(target), str(self))
