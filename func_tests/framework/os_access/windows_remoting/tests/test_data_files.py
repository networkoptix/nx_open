import os

import pytest

from framework.os_access.windows_remoting.cmd import Shell
from framework.os_access.windows_remoting.cmd.data_files import read_bytes, write_bytes


@pytest.mark.slow
def test_read_bytes(protocol):
    with Shell(protocol) as shell:
        path = r'C:\ReadRemotely.dat'
        data_to_write = os.urandom(1000 * 1000)
        write_bytes(shell, path, data_to_write)
        read_data = read_bytes(shell, path)
    assert read_data == data_to_write


@pytest.mark.slow
def test_write_bytes(protocol):
    with Shell(protocol) as shell:
        write_bytes(shell, r'C:\WrittenRemotely.dat', os.urandom(20 * 1000 * 1000))
