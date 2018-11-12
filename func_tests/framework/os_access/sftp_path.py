import errno
import functools
import io
import logging
import stat
import sys

import paramiko
import pathlib2 as pathlib
import scandir
import six

from framework.method_caching import cached_property
from framework.os_access import exceptions
from framework.os_access.path import FileSystemPath
from framework.os_access.ssh_shell import SSH

_logger = logging.getLogger(__name__)


def _reraise(errno_to_exc_cls, failure_exc_cls, func):
    @functools.wraps(func)
    def decorated_bound_method(*args, **kwargs):
        try:
            return func(*args, **kwargs)
        except IOError as e:
            if e.errno is None and failure_exc_cls is not None:
                six.reraise(failure_exc_cls, failure_exc_cls(e.errno, e.strerror),
                            sys.exc_info()[2])
            if e.errno not in errno_to_exc_cls:
                raise
            exc_cls = errno_to_exc_cls[e.errno]
            exc = exc_cls(e.errno, e.strerror)
            six.reraise(exc_cls, exc, sys.exc_info()[2])

    return decorated_bound_method


_on_existing = {
    errno.ENOENT: exceptions.DoesNotExist,
    errno.EISDIR: exceptions.NotAFile,
    errno.ENOTEMPTY: exceptions.NotEmpty,
    }
_on_new = {
    errno.ENOENT: exceptions.BadParent,
    }


def _wrap_unitary(client_bound_method):
    @functools.wraps(client_bound_method)
    def accessor_method(path, *args):
        return client_bound_method(str(path), *args)

    return accessor_method


def _wrap_binary(client_bound_method):
    @functools.wraps(client_bound_method)
    def accessor_method(path, another_path, *args):
        return client_bound_method(str(path), str(another_path), *args)

    return accessor_method


class _SftpDirEntry(object):
    def __init__(self, attrs):
        self.name = attrs.filename
        self._attrs = attrs

    def is_dir(self):
        return stat.S_ISDIR(self._attrs.st_mode)


class _SftpAccessor(object):
    def __init__(self, sftp_client):  # type: (paramiko.SFTPClient) -> ...
        self._client = sftp_client
        self.readlink = sftp_client.readlink

        self.stat = _reraise(_on_existing, None, _wrap_unitary(sftp_client.stat))
        self.lstat = _reraise(_on_existing, None, _wrap_unitary(sftp_client.lstat))
        self.listdir = _reraise(_on_existing, None, _wrap_unitary(sftp_client.listdir))
        self.chmod = _reraise(_on_existing, None, _wrap_unitary(sftp_client.chmod))
        self.mkdir = _reraise(_on_new, exceptions.AlreadyExists, _wrap_unitary(sftp_client.mkdir))
        self.rmdir = _reraise(_on_existing, exceptions.NotEmpty, _wrap_unitary(sftp_client.rmdir))
        self.rename = _reraise(_on_existing, None, _wrap_binary(sftp_client.rename))
        self._symlink = _reraise(_on_new, exceptions.AlreadyExists, _wrap_binary(sftp_client.symlink))
        self.utime = _reraise(_on_existing, None, _wrap_unitary(sftp_client.utime))
        self.unlink = _reraise(_on_existing, exceptions.NotAFile, _wrap_unitary(sftp_client.remove))

    def scandir(self, path):
        for attrs in self._client.listdir_attr(str(path)):
            dir_entry = scandir.GenericDirEntry(str(path), attrs.filename)
            dir_entry._stat = dir_entry._lstat = attrs
            yield dir_entry

    def readlink(self, path_str):
        return self._client.readlink(path_str)

    def symlink(self, target, path, target_is_directory=None):
        if target_is_directory is not None:
            _logger.warning("`target_is_directory` is ignored but given: %r", target_is_directory)
        self._symlink(target, path)


class SftpPath(FileSystemPath):
    _accessor = None  # type: _SftpAccessor
    _flavour = pathlib._posix_flavour

    @classmethod
    def specific_cls(cls, ssh):  # type: (SSH) -> ...
        class SpecificSftpPath(cls):
            @cached_property
            def _accessor(self):
                return _SftpAccessor(ssh._sftp())

            @classmethod
            def home(cls):
                return cls(ssh.home_dir(ssh.current_user_name()))

            @classmethod
            def tmp(cls):
                return cls('/tmp/func_tests')

        return SpecificSftpPath

    def open(self, mode='r', buffering=-1, encoding=None, errors=None, newline=None):
        """Open file in stream mode. Seeking is not possible. If opened for writing, file is
        truncated. Can be used not only for regular files."""
        try:
            f = self._accessor._client.open(str(self), mode, bufsize=buffering)
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
                    raise exceptions.BadParent(errno.ENOTDIR, e.strerror)
                raise
            raise
        if stat.S_ISDIR(f.stat().st_mode):
            raise exceptions.NotAFile(errno.EISDIR, 'Probably a dir')
        if 'b' in mode:
            return f
        return io.TextIOWrapper(f, encoding=encoding, errors=errors, newline=newline)
