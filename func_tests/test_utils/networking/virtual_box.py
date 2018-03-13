import csv
import subprocess

from netaddr import EUI, IPNetwork

SLOTS = [2, 3, 4, 5, 6, 7, 8]


def _get_vm_properties(vm_name):
    command = ['VBoxManage', 'showvminfo', vm_name, '--machinereadable']
    output = subprocess.check_output(command)
    return dict(csv.reader(output.splitlines(), delimiter='=', escapechar='\\', doublequote=False))


class VirtualBoxNodeNetworking(object):
    def __init__(self, vm_name):
        self._vm_name = vm_name
        properties = _get_vm_properties(self._vm_name)
        self.host_bound_mac_address = EUI(properties['macaddress1'])
        self.available_mac_addresses = [
            EUI(properties['macaddress{}'.format(slot)])
            for slot in SLOTS]
        # See: https://www.virtualbox.org/manual/ch09.html#changenat
        if properties['natnet1'] == 'nat':
            host_network = IPNetwork('10.0.2.0/24')
        else:
            host_network = IPNetwork(properties['natnet1'])
        self.host_ip_address = host_network.network + 2

    def unplug_all(self):
        for slot in SLOTS:
            key = 'nic{}'.format(slot)
            subprocess.check_call(['VBoxManage', 'controlvm', self._vm_name, key, 'null'])

    def plug(self, network_name):
        _properties = _get_vm_properties(self._vm_name)
        for slot in SLOTS:
            nic_key = 'nic{}'.format(slot)
            if _properties[nic_key] == 'null':
                mac_address_key = 'macaddress{}'.format(slot)
                mac_address = EUI(_properties[mac_address_key])
                subprocess.check_call(['VBoxManage', 'controlvm', self._vm_name, nic_key, 'intnet', network_name])
                return mac_address
        raise Exception("All adapters are busy on %r" % self._vm_name)
