import os

import pytest

from framework.os_access.windows_remoting.cmd import Shell
from framework.os_access.windows_remoting.cmd.data_files import read_bytes, write_bytes


@pytest.mark.parametrize(
    'generate_data',
    [
        lambda: os.urandom(1000 * 1000),
        lambda: 'a\0' + 'b' * 1000000,
        lambda: '\0' * 1000000,
        lambda: '\x7F' * 1000000,
        lambda: '\xFF' * 1000000,
        ],
    ids=[
        'random',
        'zero_in_the_beginning',
        'chr0_x1000000',
        'chr7f_x1000000',
        'chrFF_x1000000',
        ])
@pytest.mark.parametrize('repetitions', [1, 2], ids=['1rep', '2reps'])
def test_write_read_bytes(pywinrm_protocol, generate_data, repetitions):
    with Shell(pywinrm_protocol) as shell:
        path = r'C:\WrittenRemotely.dat'
        for _ in range(repetitions):
            data_to_write = generate_data()
            write_bytes(shell, path, data_to_write)
            read_data = read_bytes(shell, path)
            assert read_data == data_to_write
