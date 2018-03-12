"""Test coredump collection"""

import os

import pytest

from test_utils.utils import wait_until


@pytest.mark.skipif(not os.environ.has_key('FRAMEWORK_TEST'), reason='Only for testing of testing framework itself')
def test_coredump_one(server_factory):
    server = server_factory.create('bad-server')
    server.service.make_core_dump()


# this test works only for VM installations; physical servers do not restart automatically
@pytest.mark.skipif(not os.environ.has_key('FRAMEWORK_TEST'), reason='Only for testing of testing framework itself')
def test_coredump_two(server_factory):
    server = server_factory.create('bad-server')
    server.service.make_core_dump()  # first coredump
    wait_until(server.is_online)
    server.service.make_core_dump()  # second coredump
