import os

from framework.os_access.windows_remoting.cmd import Shell
from framework.os_access.windows_remoting.cmd.data_files import read_bytes, write_bytes


def test_write_read_bytes(pywinrm_protocol):
    with Shell(pywinrm_protocol) as shell:
        path = r'C:\WrittenRemotely.dat'
        data_to_write = os.urandom(1000 * 1000)
        write_bytes(shell, path, data_to_write)
        read_data = read_bytes(shell, path)
    assert read_data == data_to_write
