import os
from errno import EEXIST, EISDIR, ENOENT, ENOTDIR
from functools import wraps
from shutil import rmtree

from pathlib2 import PosixPath

from framework.os_access.exceptions import AlreadyExists, BadParent, DirIsAFile, DoesNotExist, NotADir, NotAFile
from framework.os_access.path import FileSystemPath


def _reraising(raised_errors_cls_map):
    def decorator(func):
        @wraps(func)
        def decorated(self, *args, **kwargs):
            try:
                return func(self, *args, **kwargs)
            except (IOError, OSError) as caught_error:
                # Avoid try-except with KeyError to be able to reraise it.
                if caught_error.errno in raised_errors_cls_map:
                    raised_error_cls = raised_errors_cls_map[caught_error.errno]
                    raise raised_error_cls(caught_error)
                raise

        return decorated

    return decorator


_reraising_new_file_errors = _reraising({
    ENOENT: BadParent,
    ENOTDIR: BadParent,
    EEXIST: AlreadyExists,
    EISDIR: NotAFile,
    })
_reraising_existing_file_errors = _reraising({
    ENOENT: DoesNotExist,
    EISDIR: NotAFile,
    })
_reraising_existing_dir_errors = _reraising({
    ENOENT: DoesNotExist,
    EEXIST: AlreadyExists,
    EISDIR: DirIsAFile,
    })


class LocalPath(PosixPath, FileSystemPath):
    """Access local filesystem with unified interface (incl. exceptions)

    Unlike PosixShellPath and SMBPath, there can be only one local file system,
    therefore, this class is not to be inherited from.
    """

    @classmethod
    def tmp(cls):
        temp_dir = cls('/tmp/func_tests')
        temp_dir.mkdir(parents=True, exist_ok=True)
        return temp_dir

    mkdir = _reraising_new_file_errors(PosixPath.mkdir)
    write_text = _reraising_new_file_errors(PosixPath.write_text)
    read_text = _reraising_existing_file_errors(PosixPath.read_text)
    read_bytes = _reraising_existing_file_errors(PosixPath.read_bytes)
    unlink = _reraising_existing_file_errors(PosixPath.unlink)

    def __repr__(self):
        return 'LocalPath({!r})'.format(str(self))

    def glob(self, pattern):
        if not self.exists():
            raise DoesNotExist(self)
        if not self.is_dir():
            raise NotADir(self)
        return super(LocalPath, self).glob(pattern)

    @_reraising_existing_dir_errors
    def rmtree(self, ignore_errors=False):
        rmtree(str(self), ignore_errors=ignore_errors)

    @_reraising_new_file_errors
    def write_bytes(self, data, offset=None):
        if offset is None:
            return super(LocalPath, self).write_bytes(data)
        else:
            fd = os.open(str(self), os.O_CREAT | os.O_WRONLY)
            try:
                os.lseek(fd, offset, os.SEEK_SET)
                return os.write(fd, data)
            finally:
                os.close(fd)
