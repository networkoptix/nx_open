"""Test coredump collection"""
from framework.waiting import wait_for_true


def test_make(one_running_mediaserver):
    pid = one_running_mediaserver.service.status().pid
    one_running_mediaserver.os_access.make_core_dump(pid)
    assert len(one_running_mediaserver.installation.list_core_dumps()) == 1
    wait_for_true(one_running_mediaserver.is_online)
