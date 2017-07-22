'Test to check if coredump collection works'

import os
import pytest


@pytest.mark.skipif(not os.environ.has_key('KILL_TEST'), reason='Only for testing of testing framework itself')
def test_coredump_one(server_factory):
    server = server_factory('bad-server')
    server.make_core_dump()

@pytest.mark.skipif(not os.environ.has_key('KILL_TEST'), reason='Only for testing of testing framework itself')
def test_coredump_two(server_factory):
    server = server_factory('bad-server')
    server.make_core_dump()  # first coredump
    server.wait_for_server_become_online()
    server.make_core_dump()  # second coredump
