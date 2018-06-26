import datetime

import pytest
import pytz

from framework.networking.interface import Networking
from framework.os_access.os_access_interface import OSAccess
from framework.os_access.path import FileSystemPath


@pytest.fixture(scope='session')
def os_access(one_vm):
    return one_vm.os_access


def test_run_command(os_access):
    assert os_access.run_command(['whoami'])  # I.e. something is returned.


def test_is_accessible(os_access):
    assert isinstance(os_access.is_accessible(), bool)


def test_path(os_access):  # type: (OSAccess) -> None
    assert issubclass(os_access.Path, FileSystemPath)
    assert os_access.Path is os_access.Path,  "Same class must be returned each time"
    assert isinstance(os_access.Path.home(), os_access.Path)
    assert isinstance(os_access.Path.tmp(), os_access.Path)


def test_networking(os_access):
    assert isinstance(os_access.networking, Networking)
    assert os_access.networking is os_access.networking  # I.e. same class returned each time.


def test_get_set_time(os_access):
    os_access.get_time()
    os_access.set_time(datetime.datetime.now(pytz.utc) - datetime.timedelta(days=100))
