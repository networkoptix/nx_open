import pytest

from framework.registry import Registry, RegistryError

pytest_plugins = ['fixtures.ad_hoc_ssh']


@pytest.fixture()
def registry(ad_hoc_ssh_access):
    path = ad_hoc_ssh_access.Path('/tmp/func_tests/test_registry.yaml')
    path.parent.mkdir(exist_ok=True)
    ad_hoc_ssh_access.run_command(['rm', '-f', path])
    return Registry(ad_hoc_ssh_access, path, 'test-name-{index}', 3)


def test_reserve_two(registry):
    with registry.taken('a') as (a_index, a_name):
        with registry.taken('b') as (b_index, b_name):
            assert a_name != b_name


def test_reserve_many(registry):
    with registry.taken('a'), registry.taken('b'), registry.taken('c'):
        with pytest.raises(RegistryError):
            with registry.taken('d'):
                pass
