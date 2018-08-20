import pytest

from framework.os_access.local_access import local_access
from framework.registry import Registry, RegistryError


@pytest.fixture()
def os_access():
    return local_access


@pytest.fixture()
def registry_path(node_dir):
    return node_dir / 'test_registry.yaml'


@pytest.fixture()
def registry(os_access, registry_path):
    return Registry(os_access, registry_path, 'test-name-{index:02d}'.format, 3)


def test_reserve_two(registry):
    with registry.taken('a') as (a_index, a_name):
        with registry.taken('b') as (b_index, b_name):
            assert a_name != b_name


def test_reserve_many(registry):
    with registry.taken('a'), registry.taken('b'), registry.taken('c'):
        with pytest.raises(RegistryError):
            with registry.taken('d'):
                pass


def test_reserve_two_interweave(registry):
    a_take = registry.taken('a')
    b_take = registry.taken('b')
    a_take.__enter__()
    b_take.__enter__()
    a_take.__exit__(None, None, None)
    b_take.__exit__(None, None, None)


def test_another_instance_on_same_file(registry_path, os_access):
    # Creation is tested too.
    instance_1 = registry(os_access, registry_path)
    with instance_1.taken('a') as vm_1:
        vm_1_index, vm_1_name = vm_1
        instance_2 = registry(os_access, registry_path)
        with instance_2.taken('a') as vm_2:  # Same names must be supported.
            vm_2_index, vm_2_name = vm_2
            assert vm_1_index != vm_2_index
            assert vm_1_name != vm_2_name
