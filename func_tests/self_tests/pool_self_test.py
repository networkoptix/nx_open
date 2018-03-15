def test_get(linux_vm_pool):
    machine = linux_vm_pool.get('oi')
    assert machine.alias == 'oi'
