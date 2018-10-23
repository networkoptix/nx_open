import datetime

import pytest
import pytz

from framework.networking.interface import Networking
from framework.os_access.os_access_interface import OSAccess
from framework.os_access.path import FileSystemPath


@pytest.fixture
def os_access(one_vm):
    return one_vm.os_access


def test_run_command(os_access):
    assert os_access.run_command(['whoami'])  # I.e. something is returned.


def test_is_accessible(os_access):
    assert isinstance(os_access.is_accessible(), bool)


def test_path(os_access):  # type: (OSAccess) -> None
    assert issubclass(os_access.path_cls, FileSystemPath)
    assert os_access.path_cls is os_access.path_cls, "Same class must be returned each time"
    assert isinstance(os_access.path_cls.home(), os_access.path_cls)
    assert isinstance(os_access.path_cls.tmp(), os_access.path_cls)


def test_networking(os_access):
    assert isinstance(os_access.networking, Networking)
    assert os_access.networking is os_access.networking  # I.e. same class returned each time.


def test_get_set_time(os_access):
    os_access.time.get()
    os_access.time.set(datetime.datetime.now(pytz.utc) - datetime.timedelta(days=100))


def test_disk_space(os_access):
    before = os_access.free_disk_space_bytes()
    should_be = 1000 * 1000 * 1000
    with os_access.free_disk_space_limited(should_be):
        assert os_access.free_disk_space_bytes() == pytest.approx(should_be, rel=0.01)
    assert os_access.free_disk_space_bytes() == pytest.approx(before, rel=0.01)


def test_disk_space_limit_twice(os_access):
    before = os_access.free_disk_space_bytes()
    should_be_1 = 1000 * 1000 * 1000
    with os_access.free_disk_space_limited(should_be_1):
        assert os_access.free_disk_space_bytes() == pytest.approx(should_be_1, rel=0.01)
        should_be_2 = 500 * 1000 * 1000
        with os_access.free_disk_space_limited(should_be_2):
            assert os_access.free_disk_space_bytes() == pytest.approx(should_be_2, rel=0.01)
    assert os_access.free_disk_space_bytes() == pytest.approx(before, rel=0.01)


def test_fake_disk(os_access):
    os_access.make_fake_disk('test', 100 * 1000 * 1000)
    os_access.make_fake_disk('test', 200 * 1000 * 1000)
