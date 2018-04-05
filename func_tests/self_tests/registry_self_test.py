import pytest

from framework.os_access.local import LocalAccess
from framework.registry import Registry, RegistryError


@pytest.fixture()
def os_access():
    return LocalAccess()


@pytest.fixture()
def registry(os_access, work_dir):
    path = work_dir / 'test_registry.yaml'
    os_access.run_command(['rm', '-f', path])
    return Registry(os_access, path, 'test-name-{index}', 3)


def test_free_unknown(registry):
    with pytest.raises(RegistryError):
        registry.free('unknown-name')


def test_free_already_freed(registry):
    _, name = registry.take('some-alias')
    registry.free(name)
    with pytest.raises(RegistryError):
        registry.free(name)


def test_reserve_two(registry):
    a_name = registry.take('a')
    b_name = registry.take('b')
    assert a_name != b_name


def test_reserve_many(registry):
    _ = registry.take('a')
    _ = registry.take('b')
    _ = registry.take('c')
    with pytest.raises(RegistryError):
        _ = registry.take('d')
