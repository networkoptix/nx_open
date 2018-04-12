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
        self._ssh_access = ssh_access
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
def localhost():
    user_private_key = os.path.join(
           os.environ['HOME'], '.ssh', 'id_rsa')
    return SSHAccess(
        'localhost', 22,
        username=getpass.getuser(),
        private_key_path=user_private_key)


@pytest.fixture
def localhost_files(localhost):
    temp_file_catory = TempFileFactory(localhost)
    yield temp_file_catory
    temp_file_catory.release()


def test_commands(localhost, localhost_files):
    localhost.run_command(['/bin/echo', 'Hello'])
    localhost.run_command(['/bin/echo', 'Hello, world!'])
    path_1 = localhost_files.write_file('My test file 1', 'Test')
    path_2 = localhost_files.write_file('My test file 2', 'Test')
    assert localhost.file_exists(path_1)
    assert localhost.file_exists(path_2)
    localhost.run_command(['diff', '--recursive', '--report-identical-files', path_1, path_2])
