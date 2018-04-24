import shutil
from pprint import pprint

from pathlib2 import PosixPath

from framework.os_access.path import FileSystemPath


class LocalPath(PosixPath, FileSystemPath):
    @classmethod
    def tmp(cls):
        return cls('/tmp/func_tests')

    def upload(self, local_source_path):
        shutil.copyfile(str(local_source_path), str(self))

    def download(self, local_destination_path):
        shutil.copyfile(str(self), str(local_destination_path))

    def rmtree(self, ignore_errors=False):
        shutil.rmtree(str(self), ignore_errors=ignore_errors)


if __name__ == '__main__':
    pprint(LocalPath('oi', 'ai'))
    pprint(LocalPath.tmp())
    pprint(LocalPath())
    pprint(LocalPath.__mro__)
