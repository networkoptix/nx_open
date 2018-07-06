import pytest

from framework.os_access.exceptions import DoesNotExist
from framework.registry import Registry, RegistryError

pytest_plugins = ['fixtures.ad_hoc_ssh']


@pytest.fixture()
def registry_path(ad_hoc_ssh_access):
    path = ad_hoc_ssh_access.Path.tmp() / 'test_registry.yaml'
    path.parent.mkdir(exist_ok=True, parents=True)
    try:
        path.unlink()
    except DoesNotExist:
        pass
    return path


@pytest.fixture()
def registry(ad_hoc_ssh_access, registry_path):
    return Registry(ad_hoc_ssh_access, registry_path, 'test-name-{index}', 3)


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


def test_another_instance_on_same_file(registry_path, ad_hoc_ssh_access):
    # Creation is tested too.
    instance_1 = registry(ad_hoc_ssh_access, registry_path)
    with instance_1.taken('a') as vm_1:
        vm_1_index, vm_1_name = vm_1
        instance_2 = registry(ad_hoc_ssh_access, registry_path)
        with instance_2.taken('a') as vm_2:  # Same names must be supported.
            vm_2_index, vm_2_name = vm_2
            assert vm_1_index != vm_2_index
            assert vm_1_name != vm_2_name
