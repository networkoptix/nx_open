import pytest

from framework.waiting import wait_for_true


@pytest.mark.parametrize(['host_nic', 'remote_address'], [('eno1', '10.0.2.103')])
def test_bridge(hypervisor, one_vm, host_nic, remote_address):
    hypervisor.plug_bridged(one_vm.name, host_nic)
    wait_for_true(
        lambda: one_vm.os_access.networking.can_reach(remote_address),
        "{} can be reached".format(remote_address))
