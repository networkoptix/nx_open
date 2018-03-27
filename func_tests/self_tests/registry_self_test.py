import pytest

from framework.os_access import LocalAccess
from framework.vm import Registry


@pytest.fixture()
def os_access():
    return LocalAccess()


@pytest.fixture()
def registry(os_access, work_dir):
    path = work_dir / 'test_registry.yaml'
    os_access.run_command(['rm', '-f', path])
    return Registry(os_access, path)


def test_not_reserved(registry):
    with pytest.raises(Registry.NotReserved):
        registry.relinquish(0)


def test_reserved_successfully(registry):
    a_index = registry.reserve('a')
    b_index = registry.reserve('b')
    assert a_index != b_index
