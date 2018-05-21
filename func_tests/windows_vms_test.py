#!/usr/bin/env python
import abc
import argparse
import logging

# from multiprocessing.dummy import Pool as ThreadPool
from framework.os_access.args import cmd_command_to_script
from netaddr.ip import IPNetwork
from pathlib2 import Path

from framework.networking import setup_flat_network
from framework.os_access.local_access import LocalAccess
from framework.pool import ClosingPool
from framework.registry import Registry
from framework.serialize import load
from framework.vms.factory import VMFactory
from framework.vms.hypervisor.virtual_box import VirtualBox

DEFAULT_VMS_COUNT = 10
MAX_COMMAND_LENGTH = 50
DEFAULT_IP_NETWORK = '10.254.0.0/29'


def truncate_string(s):
    if len(s) > MAX_COMMAND_LENGTH:
        return '{0}...'.format(s[0:MAX_COMMAND_LENGTH])
    return s


class Command(object):

    @abc.abstractmethod
    def __call__(self, vm):
        pass


class CmdCommand(Command):

    def __init__(self, args):
        self.args = args

    def __call__(self, vm):
        return vm.os_access.run_cmd_command(self.args)

    def __str__(self):
        return "Windows cmd command '{cmd}'".format(
            cmd=truncate_string(cmd_command_to_script(self.args)))


class PowerShellCommand(Command):

    def __init__(self, script, variables={}):
        self.script = script
        self.variables = variables

    def __call__(self, vm):
        return vm.os_access.run_powershell_script(self.script, self.variables)

    def __str__(self):
        return "Windows power shell script '{script}'".format(
            script=truncate_string(self.script))


COMMAND_LIST = [
    CmdCommand(['whoami']),
    PowerShellCommand('$env:UserName')]


def main():
    parser = argparse.ArgumentParser(usage='%(prog)s [options]')
    parser.add_argument(
        '--vms', default=DEFAULT_VMS_COUNT, type=int,
        help='VMs count. Default is %d' % DEFAULT_VMS_COUNT)
    parser.add_argument(
        '--clean-up', action='store_true',
        help="Remove all existing vms.")
    parser.add_argument('--verbosity', '-v', action='count')
    args = parser.parse_args()
    log_level = {
        1: logging.WARNING,
        2: logging.INFO,
        3: logging.DEBUG}.get(args.verbosity, logging.ERROR)
    log_format = '%(asctime)-15s %(levelname)-7s %(message)s'
    logging.basicConfig(level=log_level, format=log_format)
    host_os_access = LocalAccess()
    configuration = load((Path(__file__).parent / 'fixtures' / 'configuration.yaml').read_text())
    hypervisor = VirtualBox(host_os_access, configuration['vm_host']['address'])
    vm_registries = {
        vm_type: Registry(
            hypervisor.host_os_access,
            hypervisor.host_os_access.Path(vm_type_configuration['registry_path']).expanduser(),
            vm_type_configuration['name_format'].format(vm_index='{index}'),
            vm_type_configuration['limit'],
            )
        for vm_type, vm_type_configuration in configuration['vm_types'].items()}
    vm_factory = VMFactory(configuration['vm_types'], hypervisor, vm_registries)
    if args.clean_up:
        vm_factory.cleanup()
    with ClosingPool(vm_factory.allocated_vm, {}, 'windows') as vm_pool:

        def allocate_vm(index):
            return vm_pool.get('windows-vm-{index}'.format(index=index))

        vms = map(allocate_vm, range(args.vms))
        # Unfortunately, we can't create vm in parallel now, because of MoveLock
        # thread_pool = ThreadPool(args.vms)
        # vms = thread_pool.map(allocate_vm, range(args.vms)).get()
        # thread_pool.close()
        # thread_pool.join()
        setup_flat_network(vms, IPNetwork(DEFAULT_IP_NETWORK), hypervisor)
        # Run commands
        for cmd in COMMAND_LIST:
            logging.log(logging.INFO, "Start executing {cmd}".format(cmd=cmd))
            for vm in vms:
                logging.log(logging.DEBUG, "VM#{vm} execute {cmd}: '{result}'".format(
                    vm=vm.name, cmd=cmd, result=cmd(vm)))
            logging.log(logging.INFO, "Command execution finished")


if __name__ == "__main__":
    main()
