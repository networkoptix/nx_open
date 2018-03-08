import pytest

from test_utils.os_access import LocalAccess
from test_utils.vm import Registry


@pytest.fixture()
def os_access():
    return LocalAccess()


@pytest.fixture()
def registry(os_access, work_dir, limit):
    base_dir = work_dir / 'test_registry'
    os_access.mk_dir(base_dir)
    for path in os_access.iter_dir(base_dir):
        os_access.run_command(['rm', path])
    return Registry(os_access, base_dir, limit=limit)


@pytest.mark.parametrize('limit', [2])
def test_registry(registry):
    a = registry.reserve('a')
    b = registry.reserve('b')
    with pytest.raises(Registry.NothingAvailable):
        registry.reserve('c')
    registry.relinquish(a)
    c = registry.reserve('c')
    assert c == a
