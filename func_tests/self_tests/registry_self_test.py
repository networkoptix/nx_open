import pytest

from test_utils.os_access import LocalAccess
from test_utils.vm import Registry


@pytest.fixture()
def os_access():
    return LocalAccess()


@pytest.fixture()
def two_names_registry(os_access, work_dir):
    path = work_dir / 'test_registry.yaml'
    os_access.run_command(['rm', '-f', path])
    return Registry(os_access, path, 2, 'test-prefix-{}'.format)


@pytest.fixture()
def full_registry(two_names_registry):
    _, _ = two_names_registry.reserve('test-name-a')
    _, _ = two_names_registry.reserve('test-name-b')
    return two_names_registry


def test_no_available_names(full_registry):
    with pytest.raises(Registry.NoAvailableNames):
        _, _ = full_registry.reserve('c')


def test_not_reserved(two_names_registry):
    with pytest.raises(Registry.NotReserved):
        two_names_registry.relinquish('c')


def test_reserved_successfully(two_names_registry):
    _, _ = two_names_registry.reserve('a')
    _, _ = two_names_registry.reserve('b')


def test_reuse(two_names_registry):
    _, a_name = two_names_registry.reserve('a')
    _, _ = two_names_registry.reserve('b')
    two_names_registry.relinquish(a_name)
    _, _ = two_names_registry.reserve('c')
