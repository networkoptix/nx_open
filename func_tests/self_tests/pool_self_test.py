from contextlib import closing

from test_utils.vm import Pool


def test_get(linux_vm_pool):
    first = linux_vm_pool.get('first')
    assert first.alias == 'first'
    second = linux_vm_pool.get('second')
    assert second.alias == 'second'
    first_again = linux_vm_pool.get('first')
    assert first.name == first_again.name


def test_close(registries, vm_factories):
    with closing(Pool(registries['linux'], vm_factories['linux'])) as pool:
        first = pool.get('first')
        first_name = first.name
    with closing(Pool(registries['linux'], vm_factories['linux'])) as pool:
        second = pool.get('second')
        second_name = second.name
    assert first_name == second_name
