import errno
import os
import sys
from errno import EEXIST, EISDIR, ENOENT, ENOTDIR, ENOTEMPTY
from functools import wraps

import pathlib2 as pathlib
import six

from framework.os_access import exceptions
from framework.os_access.path import FileSystemPath


def _reraising(raised_errors_cls_map):
    def decorator(func):
        @wraps(func)
        def decorated(*args, **kwargs):
            try:
                return func(*args, **kwargs)
            except (IOError, OSError) as caught_error:
                # Avoid try-except with KeyError to be able to reraise it.
                if caught_error.errno in raised_errors_cls_map:
                    error_cls = raised_errors_cls_map[caught_error.errno]
                    error = error_cls(caught_error.errno, caught_error.strerror)
                    six.reraise(error_cls, error, sys.exc_info()[2])
                raise

        return decorated

    return decorator


_reraising_new_file_errors = _reraising({
    ENOENT: exceptions.BadParent,
    ENOTDIR: exceptions.BadParent,
    EEXIST: exceptions.AlreadyExists,
    EISDIR: exceptions.NotAFile,
    })
_reraising_existing_file_errors = _reraising({
    ENOENT: exceptions.DoesNotExist,
    EISDIR: exceptions.NotAFile,
    EEXIST: exceptions.AlreadyExists,
    ENOTEMPTY: exceptions.NotEmpty,
    })


class _Accessor(object, pathlib._NormalAccessor):
    scandir = staticmethod(_reraising_existing_file_errors(pathlib._NormalAccessor.scandir))
    listdir = staticmethod(_reraising_existing_file_errors(pathlib._NormalAccessor.listdir))
    stat = staticmethod(_reraising_existing_file_errors(pathlib._NormalAccessor.stat))
    lstat = staticmethod(_reraising_existing_file_errors(pathlib._NormalAccessor.lstat))
    chmod = staticmethod(_reraising_existing_file_errors(pathlib._NormalAccessor.chmod))
    lchmod = staticmethod(_reraising_existing_file_errors(pathlib._NormalAccessor.lchmod))
    unlink = staticmethod(_reraising_existing_file_errors(pathlib._NormalAccessor.unlink))
    rename = staticmethod(_reraising_existing_file_errors(pathlib._NormalAccessor.rename))
    mkdir = staticmethod(_reraising_new_file_errors(pathlib._NormalAccessor.mkdir))
    symlink = staticmethod(_reraising_new_file_errors(pathlib._NormalAccessor.symlink))
    utime = staticmethod(_reraising_existing_file_errors(pathlib._NormalAccessor.utime))
    readlink = _reraising_existing_file_errors(pathlib._NormalAccessor.readlink)
    rmdir = staticmethod(_reraising_existing_file_errors(pathlib._NormalAccessor.rmdir))


class LocalPath(FileSystemPath):
    """Access local filesystem with unified interface (incl. exceptions)

    Unlike SftpPath and SMBPath, there can be only one local file system,
    therefore, this class is not to be inherited from.
    """

    _accessor = _Accessor()
    _flavour = pathlib._windows_flavour if os.name == 'nt' else pathlib._posix_flavour

    def open(self, mode='r', buffering=-1, encoding=None, errors=None, newline=None):
        super_open = super(LocalPath, type(self)).open
        if 'w' in mode or '+' in mode or 'a' in mode:
            cls_open = _reraising_new_file_errors(super_open)
        else:
            cls_open = _reraising_existing_file_errors(super_open)
        return cls_open(
            self,
            mode=mode,
            buffering=buffering,
            encoding=encoding, errors=errors, newline=newline)

    @classmethod
    def tmp(cls):
        temp_dir = cls('/tmp/func_tests')
        temp_dir.mkdir(parents=True, exist_ok=True)
        return temp_dir

    def __repr__(self):
        return 'LocalPath({!r})'.format(str(self))

    def take_from(self, local_source_path):
        destination = self / local_source_path.name
        if not local_source_path.exists():
            raise exceptions.CannotDownload("Local file {} doesn't exist.".format(local_source_path))
        try:
            os.symlink(str(local_source_path), str(destination))
        except OSError as e:
            if e.errno != errno.EEXIST:
                raise
            return destination
        return destination
