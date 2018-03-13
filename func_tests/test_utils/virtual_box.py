import csv
import logging
from collections import namedtuple
from itertools import count

from test_utils.networking.virtual_box import SLOTS
from test_utils.os_access import ProcessError
from test_utils.utils import wait_until

logger = logging.getLogger(__name__)

VMInfo = namedtuple('VMInfo', ['name', 'ports', 'is_running'])


class VirtualBox(object):
    """Interface for VirtualBox as hypervisor."""

    def __init__(self, os_access, hostname):
        self.hostname = hostname
        self.os_access = os_access

    def _info(self, name):
        try:
            output = self.os_access.run_command(['VBoxManage', 'showvminfo', name, '--machinereadable'])
        except ProcessError as e:
            if e.returncode == 1:
                raise VMNotFound("Cannot find VM {}; VBoxManage says:\n{}".format(name, e.output))
            raise
        raw_dict = dict(csv.reader(output.splitlines(), delimiter='=', escapechar='\\', doublequote=False))
        ports = {}
        for index in count():
            try:
                raw_value = raw_dict['Forwarding({})'.format(index)]
            except KeyError:
                break
            tag, protocol, host_address, host_port, guest_address, guest_port = raw_value.split(',')
            ports[protocol, int(guest_port)] = int(host_port)
        return VMInfo(name=raw_dict['name'], is_running=raw_dict['VMState'] == 'running', ports=ports)

    def find(self, name):
        return self._info(name)

    def power_on(self, name):
        self.os_access.run_command(['VBoxManage', 'startvm', name, '--type', 'headless'])

    def power_off(self, name):
        self.os_access.run_command(['VBoxManage', 'controlvm', name, 'acpipowerbutton'])

    def create(self, name, template, forwarded_ports):
        """Clone and setup VM with simple and generic parameters. Template can be any specific type."""
        self.os_access.run_command([
            'VBoxManage', 'clonevm', template['vm'],
            '--snapshot', template['snapshot'],
            '--name', name,
            '--options', 'link',
            '--register',
            ])
        settings_args = []
        settings_args += ['--nic1', 'nat', '--natnet1', '10.10.0.0/24']
        for slot in SLOTS:
            settings_args += ['--nic{}'.format(slot), 'null']
        for tag, protocol, host_port, guest_port in forwarded_ports:
            settings_args += ['--natpf1', '{},{},,{},,{}'.format(tag, protocol, host_port, guest_port)]
        self.os_access.run_command(['VBoxManage', 'modifyvm', name] + settings_args)
        self.os_access.run_command([
            'VBoxManage', 'setextradata', name,
            'VBoxInternal/Devices/VMMDev/0/Config/GetHostTimeDisabled', 1  # Don't spoil tests with time sync.
            ])
        return self._info(name)

    def destroy(self, name):
        if self.find(name).is_running:
            self.os_access.run_command(['VBoxManage', 'controlvm', name, 'poweroff'])
            assert wait_until(lambda: not self.find(name).is_running, name='until VM is off')
        self.os_access.run_command(['VBoxManage', 'unregistervm', name, '--delete'])


class VMNotFound(Exception):
    pass
