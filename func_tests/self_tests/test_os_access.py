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
    epsilon = 10 * 1000 * 1000
    before = os_access.free_disk_space_bytes()
    should_be = 1000 * 1000 * 1000
    if os_access.free_disk_space_bytes() < should_be:
        pytest.skip("Too low on space, need {} MB".format(should_be / 1024 / 1024))
    with os_access.free_disk_space_limited(should_be):
        assert os_access.free_disk_space_bytes() == pytest.approx(should_be, abs=epsilon)
    assert os_access.free_disk_space_bytes() == pytest.approx(before, abs=epsilon)


def test_disk_space_limit_twice(os_access):
    epsilon = 10 * 1000 * 1000
    before = os_access.free_disk_space_bytes()
    should_be_1 = 1000 * 1000 * 1000
    if os_access.free_disk_space_bytes() < should_be_1:
        pytest.skip("Too low on space, need {} MB".format(should_be_1 / 1024 / 1024))
    with os_access.free_disk_space_limited(should_be_1):
        assert os_access.free_disk_space_bytes() == pytest.approx(should_be_1, abs=epsilon)
        should_be_2 = 500 * 1000 * 1000
        assert should_be_2 < should_be_1, "First limitation still imposed"
        with os_access.free_disk_space_limited(should_be_2):
            assert os_access.free_disk_space_bytes() == pytest.approx(should_be_2, abs=epsilon)
    assert os_access.free_disk_space_bytes() == pytest.approx(before, abs=epsilon)


def test_fake_disk(os_access):
    size = 500 * 1000 * 1000
    mount_point = os_access.make_fake_disk('test', size)
    assert os_access._free_disk_space_bytes_on_all()[mount_point] == pytest.approx(size, rel=0.05)


def test_fake_disk_twice_same_name(os_access):
    size_1 = 500 * 1000 * 1000
    mount_point_1 = os_access.make_fake_disk('test', size_1)
    assert os_access._free_disk_space_bytes_on_all()[mount_point_1] == pytest.approx(size_1, rel=0.05)
    size_2 = 700 * 1000 * 1000
    mount_point_2 = os_access.make_fake_disk('test', size_2)
    assert os_access._free_disk_space_bytes_on_all()[mount_point_2] == pytest.approx(size_2, rel=0.05)
    assert mount_point_2 == mount_point_1


def test_fake_disk_twice_different_names(os_access):
    size_1 = 500 * 1000 * 1000
    mount_point_1 = os_access.make_fake_disk('test', size_1)
    assert os_access._free_disk_space_bytes_on_all()[mount_point_1] == pytest.approx(size_1, rel=0.05)
    size_2 = 700 * 1000 * 1000
    mount_point_2 = os_access.make_fake_disk('test_different', size_2)
    assert os_access._free_disk_space_bytes_on_all()[mount_point_2] == pytest.approx(size_2, rel=0.05)
    assert mount_point_2 == mount_point_1


def test_fake_disk_twice_less_size(os_access):
    size_1 = 700 * 1000 * 1000
    mount_point_1 = os_access.make_fake_disk('test', size_1)
    assert os_access._free_disk_space_bytes_on_all()[mount_point_1] == pytest.approx(size_1, rel=0.05)
    size_2 = 500 * 1000 * 1000
    mount_point_2 = os_access.make_fake_disk('test', size_2)
    assert os_access._free_disk_space_bytes_on_all()[mount_point_2] == pytest.approx(size_2, rel=0.05)
    assert mount_point_2 == mount_point_1
