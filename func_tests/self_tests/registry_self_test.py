import pytest
from pathlib2 import Path

from framework.os_access.local import LocalAccess
from framework.registry import Registry, RegistryError


@pytest.fixture()
def os_access():
    return LocalAccess()


@pytest.fixture()
def registry(os_access):
    path = Path('/tmp/func_tests/test_registry.yaml')
    path.parent.mkdir(exist_ok=True)
    os_access.run_command(['rm', '-f', path])
    return Registry(os_access, path, 'test-name-{index}', 3)


def test_reserve_two(registry):
    with registry.taken('a') as (a_index, a_name):
        with registry.taken('b') as (b_index, b_name):
            assert a_name != b_name


def test_reserve_many(registry):
    with registry.taken('a'):
        with registry.taken('b'):
            with registry.taken('c'):
                with pytest.raises(RegistryError):
                    with registry.taken('d'):
                        pass
