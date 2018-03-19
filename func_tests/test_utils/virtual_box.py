import csv
import logging
from collections import namedtuple
from pprint import pformat

from netaddr import EUI

from test_utils.os_access import NonZeroExitStatus
from test_utils.utils import wait_until

logger = logging.getLogger(__name__)

NETWORK_SLOTS = [2, 3, 4, 5, 6, 7, 8]

VMInfo = namedtuple('VMInfo', ['name', 'ports', 'macs', 'networks', 'is_running'])


class VMNotFound(Exception):
    pass


class VMNoAvailableNetworkSlot(Exception):
    def __init__(self, vm_name, vm_networks):
        super(VMNoAvailableNetworkSlot, self).__init__(
            "No available network slot on {}:\n{}".format(vm_name, pformat(vm_networks)))
        self.vm_name = vm_name
        self.vm_networks = vm_networks


class VirtualBox(object):
    """Interface for VirtualBox as hypervisor."""

    def __init__(self, os_access, hostname):
        self.hostname = hostname
        self.os_access = os_access

    def find(self, name):
        try:
            output = self.os_access.run_command(['VBoxManage', 'showvminfo', name, '--machinereadable'])
        except NonZeroExitStatus as e:
            if e.exit_status == 1:
                raise VMNotFound("Cannot find VM {}; VBoxManage says:\n{}".format(name, e.stderr))
            raise
        raw_info = dict(csv.reader(output.splitlines(), delimiter='=', escapechar='\\', doublequote=False))
        ports = {}
        for index in range(10):  # Arbitrary.
            try:
                raw_value = raw_info['Forwarding({})'.format(index)]
            except KeyError:
                break
            tag, protocol, host_address, host_port, guest_address, guest_port = raw_value.split(',')
            # Hostname is given with port because knowledge that VMs are accessible through
            # forwarded port is not part of logical interface. Other possibility is to make virtual network
            # in which host can access VMs on IP level. One more option is special VM which is accessible by IP
            # and forwards ports to target VMs.
            ports[protocol, int(guest_port)] = self.hostname, int(host_port)
        macs = {}
        networks = {}
        for slot in NETWORK_SLOTS:
            try:
                raw_mac = raw_info['macaddress{}'.format(slot)]
            except KeyError:
                # If not present, interface is disabled ('none'). Skip this slot at all.
                continue
            macs[slot] = EUI(raw_mac)
            if raw_info['nic{}'.format(slot)] == 'null':
                networks[slot] = None
            else:
                networks[slot] = raw_info['intnet{}'.format(slot)]
        return VMInfo(raw_info['name'], ports, macs, networks, raw_info['VMState'] == 'running')

    def clone(self, name, index, template, forwarded_ports):
        """Clone and setup VM with simple and generic parameters. Template can be any specific type."""
        self.os_access.run_command([
            'VBoxManage', 'clonevm', template['vm'],
            '--snapshot', template['snapshot'],
            '--name', name,
            '--options', 'link',
            '--register',
            ])
        # Same IP spaces for NAT cause problems with merging.
        # When answering to merge, local (requested) server reports its IPs,
        # remote (which is requested from local) tries to connect all those IPs,
        # connects with shortest delay to address which is used for NAT by both VMs
        # and "thinks" it's the peer whereas it "talks" to itself.
        settings_args = [
            '--nic1', 'nat', '--natnet1', '192.168.{:d}.0/24'.format(index),
            '--paravirtprovider', 'kvm',
            '--cpuexecutioncap', 100,
            '--cpus', 4,
            '--memory', 4 * 1024]
        for tag, protocol, host_port, guest_port in forwarded_ports:
            settings_args += ['--natpf1', '{},{},,{},,{}'.format(tag, protocol, host_port, guest_port)]
        for slot in NETWORK_SLOTS:
            settings_args += ['--nic{}'.format(slot), 'null']
        self.os_access.run_command(['VBoxManage', 'modifyvm', name] + settings_args)
        extra_data = {'VBoxInternal/Devices/VMMDev/0/Config/GetHostTimeDisabled': 1}
        for extra_key, extra_value in extra_data.items():
            self.os_access.run_command(['VBoxManage', 'setextradata', name, extra_key, extra_value])
        return self.find(name)

    def destroy(self, name):
        if self.find(name).is_running:
            self.os_access.run_command(['VBoxManage', 'controlvm', name, 'poweroff'])
            assert wait_until(lambda: not self.find(name).is_running, name='until VM is off')
        self.os_access.run_command(['VBoxManage', 'unregistervm', name, '--delete'])

    def power_on(self, name):
        self.os_access.run_command(['VBoxManage', 'startvm', name, '--type', 'headless'])

    def power_off(self, name):
        self.os_access.run_command(['VBoxManage', 'controlvm', name, 'poweroff'])

    def plug(self, name, network):
        info = self.find(name)
        try:
            slot = next(slot for slot in info.networks if info.networks[slot] is None)
        except StopIteration:
            raise VMNoAvailableNetworkSlot(name, info.networks)
        self.os_access.run_command(['VBoxManage', 'controlvm', name, 'nic{}'.format(slot), 'intnet', network])
        return info.macs[slot]

    def unplug_all(self, name):
        info = self.find(name)
        for slot in info.networks:
            if info.networks[slot] is not None:
                self.os_access.run_command(['VBoxManage', 'controlvm', name, 'nic{}'.format(slot), 'null'])
