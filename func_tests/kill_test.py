'Test to check if coredump collection works'

import os
import pytest


@pytest.mark.skipif(not os.environ.has_key('KILL_TEST'), reason='Only for testing of testing framework itself')
def test_coredump(server_factory):
    server = server_factory('bad-server')
    server.host.run_command(['killall', '--signal', 'SIGTRAP', 'mediaserver-bin'])  # first coredump
    server.wait_for_server_become_online()
    server.host.run_command(['killall', '--signal', 'SIGTRAP', 'mediaserver-bin'])  # second coredump
