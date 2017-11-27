'''Configuration for vagrant boxes.

Functional tests define configuration required for them indirectly (via 'box' fixture) using BoxConfig class.
'''

import os.path

from netaddr import IPNetwork

from .utils import is_list_inst, quote

DEFAULT_NATNET1 = '10.10.0/24'
DEFAULT_PRIVATE_NET = '10.10.1/24'
MEDIASERVER_LISTEN_PORT = 7001
BOX_PROVISION_MEDIASERVER = 'box-provision-mediaserver.sh'
SSH_PORT_OFFSET=2220


class ConfigCommand(object):

    @classmethod
    def from_dict(cls, d):
        return cls(function=d['function'],
                   args=d['args'],
                   kwargs=d['kwargs'])

    def __init__(self, function, args, kwargs=None):
        self.function = function    # str or None
        self.args = args            # str list
        self.kwargs = kwargs or {}  # str->str dict

    def __eq__(self, other):
        if not isinstance(other, ConfigCommand):
            return False
        return (self.function == other.function and
                self.args == other.args and
                self.kwargs == other.kwargs)

    def __repr__(self):
        return 'ConfigCommand(%s %s %s)' % (self.function, self.args, self.kwargs)

    def to_dict(self):
        return dict(
            function=self.function,
            args=self.args,
            kwargs=self.kwargs,
            )


def make_vm_config_internal_network_command(vbox_manage, network):
    kwargs = {}
    if network.network == network.ip:  # it is just a network, and dhcp must be used
        kwargs['type'] = ':dhcp'
    else:
        kwargs['ip'] = quote(network.ip)
    network_name = vbox_manage.produce_internal_network(network)
    kwargs['virtualbox__intnet'] = quote(network_name)
    return ConfigCommand('network', [':private_network'], kwargs)

def make_vm_provision_command(script, args=None, env=None):
    kwargs = dict(path=quote(script))
    if args:
        kwargs['args'] = '[%s]' % ', '.join(quote(arg) for arg in args)
    if env:
        kwargs['env'] = '{%s}' % ', '.join('%s: %s' % (name, value) for name, value in env.items())
    return ConfigCommand('provision', [':shell'], kwargs)

def make_vbox_host_time_disabled_command():
    return ConfigCommand(None, [':setextradata', ':id', quote('VBoxInternal/Devices/VMMDev/0/Config/GetHostTimeDisabled'), '1'])


class BoxConfigFactory(object):

    def __init__(self, mediaserver_dist_path, customization_company_name):
        self._mediaserver_dist_path = mediaserver_dist_path
        self._customization_company_name = customization_company_name

    # ip_address may end with .0 (like 1.2.3.0); this will be treated as network address, and dhcp will be used for it
    def __call__(self, name=None, install_server=True, provision_scripts=None, must_be_recreated=False,
                 ip_address_list=None, required_file_list=None, sync_time=True):
        vm_commands = []
        vbox_commands = []
        ip_address_list = map(IPNetwork, ip_address_list or [DEFAULT_PRIVATE_NET])
        if not required_file_list:
            required_file_list = []
        if not sync_time:
            vbox_commands += [make_vbox_host_time_disabled_command()]
        if install_server:
            mediaserver_dist_fname = os.path.basename(self._mediaserver_dist_path)
            vm_commands += [make_vm_provision_command(
                BOX_PROVISION_MEDIASERVER,
                args=[mediaserver_dist_fname],
                env=dict(COMPANY_NAME=quote(self._customization_company_name)))]
            required_file_list += [self._mediaserver_dist_path,
                                   '{test_dir}/test_utils/' + BOX_PROVISION_MEDIASERVER]
        for script in provision_scripts or []:
            vm_commands += [make_vm_provision_command(script)]
            required_file_list.append('{test_dir}/' + script)
        if install_server:
            server_customization_company_name = self._customization_company_name
        else:
            server_customization_company_name = None
        return BoxConfig(name, ip_address_list, required_file_list, server_customization_company_name,
                         vm_commands, vbox_commands, must_be_recreated)


class BoxConfig(object):

    @classmethod
    def from_dict(cls, d):
        return cls(
            name=d['name'],
            ip_address_list=map(IPNetwork, d['ip_address_list']),
            required_file_list=d['required_file_list'],
            server_customization_company_name=d['server_customization_company_name'],
            vm_commands=[ConfigCommand.from_dict(command) for command in d['vm_commands']],
            vbox_commands=[ConfigCommand.from_dict(command) for command in d['vbox_commands']],
            idx=d['idx'],
            vm_name_prefix=d['vm_name_prefix'],
            vm_port_base=d['vm_port_base'],
            )

    def __init__(self, name, ip_address_list, required_file_list, server_customization_company_name,
                 vm_commands, vbox_commands, must_be_recreated=False,
                 idx=None, vm_name_prefix=None, vm_port_base=None):
        assert is_list_inst(ip_address_list, IPNetwork), repr(ip_address_list)
        self.name = name
        self.ip_address_list = ip_address_list
        self.required_file_list = required_file_list
        self.server_customization_company_name = server_customization_company_name  # None if server was not installed
        self.vm_commands = vm_commands
        self.vbox_commands = vbox_commands
        self.idx = idx
        self.vm_name_prefix = vm_name_prefix
        self.vm_port_base = vm_port_base
        self.must_be_recreated = must_be_recreated  # this test requires fresh box
        self.is_allocated = False

    def __str__(self):
        return '%s, %r, %r, %r, %r' % (self.idx, self.name, self.required_file_list, self.vm_commands, self.vbox_commands)

    def __repr__(self):
        return 'BoxConfig(%s)' % self

    def to_dict(self):
        return dict(
            name=self.name,
            ip_address_list=map(str, self.ip_address_list),
            required_file_list=self.required_file_list,
            server_customization_company_name=self.server_customization_company_name,
            vm_commands=[command.to_dict() for command in self.vm_commands],
            vbox_commands=[command.to_dict() for command in self.vbox_commands],
            idx=self.idx,
            vm_name_prefix=self.vm_name_prefix,
            vm_port_base=self.vm_port_base,
            )

    def clone(self, idx, vm_name_prefix, vm_port_base):
        return BoxConfig(
            name=self.name,
            ip_address_list=self.ip_address_list,
            required_file_list=self.required_file_list,
            server_customization_company_name=self.server_customization_company_name,
            vm_commands=self.vm_commands,
            vbox_commands=self.vbox_commands,
            idx=idx,
            vm_name_prefix=vm_name_prefix,
            vm_port_base=vm_port_base,
            )

    def matches(self, other):
        return (self.name == other.name
                and self.ip_address_list == other.ip_address_list
                and self.server_customization_company_name == other.server_customization_company_name
                and sorted(self.required_file_list) == sorted(other.required_file_list)
                and self.vm_commands == other.vm_commands
                and self.vbox_commands == other.vbox_commands)

    @property
    def box_name(self):
        assert self.idx is not None  # must be assigned for box_name
        if self.name:
            suffix = '-%s' % self.name
        else:
            suffix = ''
        return 'box-%d%s' % (self.idx, suffix)

    @property
    def vm_box_name(self):
        assert self.vm_name_prefix  # must be set before this call
        return self.vm_name_prefix + self.box_name

    @property
    def rest_api_forwarded_port(self):
        return self.vm_port_base + self.idx

    @property
    def ssh_forwarded_port(self):
        return self.vm_port_base + SSH_PORT_OFFSET + self.idx

    def expand(self, vbox_manage):
        network_vm_commands = [make_vm_config_internal_network_command(vbox_manage, ip_address)
                               for ip_address in self.ip_address_list]
        return ExpandedBoxConfig(
            box_name=self.box_name,
            vm_box_name=self.vm_box_name,
            rest_api_internal_port=MEDIASERVER_LISTEN_PORT,
            rest_api_forwarded_port=self.rest_api_forwarded_port,
            ssh_forwarded_port=self.ssh_forwarded_port,
            vm_commands=map(self._expand_vm_command, network_vm_commands + self.vm_commands),
            vbox_commands=map(self._expand_vbox_command, self.vbox_commands),
            )

    def _expand_vm_command(self, command):
        return 'vm.{function} {args}{kw_delimiter}{kwargs}'.format(
            function=command.function, args=', '.join(command.args),
            kw_delimiter=', ' if command.kwargs else '',
            kwargs=', '.join('%s: %s' % (name, value) for name, value in command.kwargs.items()))

    def _expand_vbox_command(self, command):
        return 'customize [{args}]'.format(args=', '.join(command.args))


class ExpandedBoxConfig(object):

    def __init__(self, box_name, vm_box_name, rest_api_internal_port, rest_api_forwarded_port,
                 ssh_forwarded_port, vm_commands, vbox_commands):
        self.box_name = box_name
        self.vm_box_name = vm_box_name
        self.rest_api_internal_port = rest_api_internal_port
        self.rest_api_forwarded_port = rest_api_forwarded_port
        self.ssh_forwarded_port = ssh_forwarded_port
        self.vm_commands = vm_commands
        self.vbox_commands = vbox_commands
