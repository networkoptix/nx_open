
import pytest
import getpass
import logging
import os
import uuid
import tempfile

from framework.os_access.ssh import SSHAccess

_logger = logging.getLogger(__name__)


class TempFileFactory(object):
    def __init__(self, ssh_access):
        self._ssh_access = ssh_access  # SSHAccess
        self._temp_dir = os.path.join(
            tempfile.gettempdir(), str(uuid.uuid4()))
        self._ssh_access.mk_dir(self._temp_dir)

    def write_file(self, filename, contents):
        filepath = os.path.join(self._temp_dir, filename)
        self._ssh_access.write_file(filepath, contents)
        return filepath

    def release(self):
        self._ssh_access.rm_tree(self._temp_dir)


@pytest.fixture
def linux_vm_files(linux_vm):
    temp_file_catory = TempFileFactory(linux_vm.os_access)
    yield temp_file_catory
    temp_file_catory.release()


def test_echo(linux_vm):
    assert linux_vm.os_access.run_command(['/bin/echo', '-n', 'Hello']) == 'Hello'
    assert linux_vm.os_access.run_command(['/bin/echo', '-n', 'Hello, world!']) == 'Hello, world!'


def test_diff_files(linux_vm, linux_vm_files):
    '''There was a bug in the SSHAccess with quoting,
    following diff command  illuminated it in tests.'''
    path_1 = linux_vm_files.write_file('My test file 1', 'Test')
    path_2 = linux_vm_files.write_file('My test file 2', 'Test')
    assert linux_vm.os_access.file_exists(path_1)
    assert linux_vm.os_access.file_exists(path_2)
    linux_vm.os_access.run_command(['diff', '--recursive', '--report-identical-files', path_1, path_2])
