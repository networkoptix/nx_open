"""Test coredump collection"""

import os

import pytest

from framework.waiting import wait_for_true


@pytest.mark.skipif(not os.environ.has_key('FRAMEWORK_TEST'), reason='Only for testing of testing framework itself')
def test_coredump_one(linux_mediaservers_pool):
    server = linux_mediaservers_pool.get('bad-server')
    server.service.make_core_dump()


# this test works only for VM installations; physical servers do not restart automatically
@pytest.mark.skipif(not os.environ.has_key('FRAMEWORK_TEST'), reason='Only for testing of testing framework itself')
def test_coredump_two(linux_mediaservers_pool):
    server = linux_mediaservers_pool.get('bad-server')
    server.service.make_core_dump()  # first coredump
    wait_for_true(server.is_online, "{} is online".format(server))
    server.service.make_core_dump()  # second coredump
