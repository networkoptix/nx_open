"""Test coredump collection"""
import pytest
import logging

from framework.os_access.windows_access import WindowsAccess
from framework.os_access.windows_core_dump import parse_dump
from framework.waiting import wait_for_true

_logger = logging.getLogger(__name__)


def test_make(one_running_mediaserver):
    pid = one_running_mediaserver.service.status().pid
    one_running_mediaserver.os_access.make_core_dump(pid)
    assert len(one_running_mediaserver.installation.list_core_dumps()) == 1
    wait_for_true(one_running_mediaserver.is_online)


def test_parse(one_running_mediaserver):
    if not isinstance(one_running_mediaserver.os_access, WindowsAccess):
        pytest.skip()
    symbols_path = (
        r'cache*;'
        # By some obscure reason, when run via WinRM, `cdb` cannot fetch `.pdb` from Microsoft Symbol Server.
        # (Same command, copied from `procmon`, works like a charm.)
        # Hope symbols exported from DLLs will suffice.
        # r'srv*;'
        # r'srv*http://msdl.microsoft.com/download/symbols;'
        r'\\cinas\beta-builds\repository\v1\develop\vms\444\default\windows-x64\bin;'
        # r'\\cinas\beta-builds\repository\v1\develop\vms\444\default\windows-x64\bin\plugins;'
        # r'\\cinas\beta-builds\repository\v1\develop\vms\444\default\windows-x64\bin\plugins_optional;'
    )
    dump_path = 'C:\Users\Administrator\AppData\Local\Temp\mediaserver (2).DMP'
    # pid = one_running_mediaserver.service.status().pid
    # one_running_mediaserver.os_access.make_core_dump(pid)
    # dump_path = one_running_mediaserver.installation.list_core_dumps()[-1]
    _logger.debug("Dump:\n%s", parse_dump(one_running_mediaserver.os_access, dump_path, symbols_path))
