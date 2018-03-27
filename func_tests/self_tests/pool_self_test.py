from contextlib import closing

from framework.pool import Pool


def test_get(linux_vms_pool):
    first = linux_vms_pool.get('first')
    assert first.alias == 'first'
    second = linux_vms_pool.get('second')
    assert second.alias == 'second'
    first_again = linux_vms_pool.get('first')
    assert first.name == first_again.name


def test_close(vm_factories):
    with closing(Pool(vm_factories['linux'])) as pool:
        first = pool.get('first')
        first_name = first.name
    with closing(Pool(vm_factories['linux'])) as pool:
        second = pool.get('second')
        second_name = second.name
    assert first_name == second_name
