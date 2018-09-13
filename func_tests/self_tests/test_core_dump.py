"""Test coredump collection"""
import logging

import pytest

from framework.os_access.exceptions import CoreDumpError
from framework.waiting import wait_for_truthy

_logger = logging.getLogger(__name__)


def test_make(one_running_mediaserver):
    pid = one_running_mediaserver.service.status().pid
    one_running_mediaserver.os_access.make_core_dump(pid)
    assert len(one_running_mediaserver.installation.list_core_dumps()) == 1
    wait_for_truthy(one_running_mediaserver.api.is_online)


def test_make_for_missing_process(linux_vm):
    guaranteed_invalid_pid = 999999
    with pytest.raises(CoreDumpError) as excinfo:
        linux_vm.os_access.make_core_dump(guaranteed_invalid_pid)
    assert str(excinfo.value).startswith('Failed to make core dump: ')


def test_parse(one_running_mediaserver):
    pid = one_running_mediaserver.service.status().pid
    one_running_mediaserver.os_access.make_core_dump(pid)
    path = one_running_mediaserver.installation.list_core_dumps()[-1]
    parsed = one_running_mediaserver.installation.parse_core_dump(path)
    _logger.info("Parsed:\n%s", parsed)
