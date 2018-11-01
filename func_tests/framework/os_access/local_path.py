import errno
import os
import stat
from errno import EEXIST, EISDIR, ENOENT, ENOTDIR, ENOTEMPTY
from functools import wraps
from shutil import rmtree

from pathlib2 import PosixPath

from framework.os_access import exceptions
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
                    raise raised_error_cls(caught_error.errno, caught_error.strerror)
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
    })
_reraising_existing_dir_errors = _reraising({
    ENOENT: exceptions.DoesNotExist,
    EEXIST: exceptions.AlreadyExists,
    ENOTEMPTY: exceptions.NotEmpty,
    })


class LocalPath(PosixPath, FileSystemPath):
    """Access local filesystem with unified interface (incl. exceptions)

    Unlike SftpPath and SMBPath, there can be only one local file system,
    therefore, this class is not to be inherited from.
    """

    @classmethod
    def tmp(cls):
        temp_dir = cls('/tmp/func_tests')
        temp_dir.mkdir(parents=True, exist_ok=True)
        return temp_dir

    mkdir = _reraising_new_file_errors(PosixPath.mkdir)
    write_bytes = _reraising_new_file_errors(PosixPath.write_bytes)
    read_bytes = _reraising_existing_file_errors(PosixPath.read_bytes)
    write_text = _reraising_new_file_errors(PosixPath.write_text)
    read_text = _reraising_existing_file_errors(PosixPath.read_text)
    unlink = _reraising_existing_file_errors(PosixPath.unlink)
    rmdir = _reraising_existing_dir_errors(PosixPath.rmdir)

    def __repr__(self):
        return 'LocalPath({!r})'.format(str(self))

    def glob(self, pattern):
        if not self.exists():
            raise exceptions.DoesNotExist(errno.ENOENT, str(self))
        if not self.is_dir():
            raise exceptions.NotADir(errno.ENOTDIR, str(self))
        return super(LocalPath, self).glob(pattern)

    @_reraising_existing_dir_errors
    def rmtree(self, ignore_errors=False):
        rmtree(str(self), ignore_errors=ignore_errors)

    @_reraising_new_file_errors
    def patch(self, offset, data):
        fd = os.open(str(self), os.O_CREAT | os.O_WRONLY)
        try:
            os.lseek(fd, offset, os.SEEK_SET)
            return os.write(fd, data)
        finally:
            os.close(fd)

    @_reraising_existing_file_errors
    def yank(self, offset, max_length=None):
        with self.open('rb') as f:
            f.seek(offset)
            return f.read(max_length)

    @_reraising_existing_file_errors
    def size(self):
        path_stat = self.stat()  # type: os.stat_result
        if not stat.S_ISREG(path_stat.st_mode):
            raise exceptions.NotAFile(errno.EISDIR, "{!r}.stat() returns {!r}".format(self, path_stat))
        return path_stat.st_size

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

    @_reraising_new_file_errors
    def symlink_to(self, target, target_is_directory=False):
        if not isinstance(target, type(self)):
            raise ValueError(
                "Symlink can only point to same OS but link is {} and target is {}".format(
                    self, target))
        super(LocalPath, self).symlink_to(
            str(target),
            target_is_directory=target_is_directory)
