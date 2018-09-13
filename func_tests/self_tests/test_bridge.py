import pytest

from framework.waiting import wait_for_truthy


@pytest.mark.parametrize(['host_nic', 'remote_address'], [('eno1', '10.0.2.103')])
def test_bridge(one_vm, host_nic, remote_address):
    one_vm.hardware.plug_bridged(host_nic)
    wait_for_truthy(
        lambda: one_vm.os_access.networking.can_reach(remote_address),
        "{} can be reached".format(remote_address))
