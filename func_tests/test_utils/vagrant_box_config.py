'''Configuration for vagrant boxes.

Functional tests define configuration required for them indirectly (via 'box' fixture) using BoxConfig class.
'''

from netaddr import IPNetwork
from .utils import is_list_inst


DEFAULT_NATNET1 = '10.10.0/24'
DEFAULT_PRIVATE_NET = '10.10.1/24'
REST_API_FORWARDED_PORT_BASE = 17000
MEDIASERVER_LISTEN_PORT = 7001
BOX_PROVISION_MEDIASERVER = 'box-provision-mediaserver.sh'
MEDIASERVER_DIST_FNAME = 'mediaserver.deb'  # expected in vagrant dir


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
            return false
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


def quote(s):
    return '"%s"' % s

# we only support address mask of 24
def make_vm_config_internal_network_command(vbox_manage, network):
    kwargs = {}
    if network.network == network.ip:  # it is just a network, and dhcp must be used
        kwargs['type'] = ':dhcp'
    else:
        kwargs['ip'] = quote(network.ip)
    network_name = vbox_manage.produce_internal_network(network)
    kwargs['virtualbox__intnet'] = quote(network_name)
    return ConfigCommand('network', [':private_network'], kwargs)

def make_vm_provision_command(script, env=None):
    kwargs = dict(path=quote(script))
    if env:
        kwargs['env'] = '{%s}' % ', '.join('%s: %s' % (name, value) for name, value in env.items())
    return ConfigCommand('provision', [':shell'], kwargs)

def make_vbox_host_time_disabled_command():
    return ConfigCommand(None, [':setextradata', ':id', quote('VBoxInternal/Devices/VMMDev/0/Config/GetHostTimeDisabled'), '1'])


class BoxConfigFactory(object):

    def __init__(self, company_name):
        self._company_name = company_name

    # ip_address may end with .0 (like 1.2.3.0); this will be treated as network address, and dhcp will be used for it
    def __call__(self, name=None, install_server=True, provision_scripts=None,
                 ip_address_list=None, required_file_list=None, sync_time=True):
        vm_commands = []
        vbox_commands = []
        ip_address_list = map(IPNetwork, ip_address_list or [DEFAULT_PRIVATE_NET])
        if not required_file_list:
            required_file_list = []
        if not sync_time:
            vbox_commands += [make_vbox_host_time_disabled_command()]
        if install_server:
            vm_commands += [make_vm_provision_command(
                BOX_PROVISION_MEDIASERVER, env=dict(COMPANY_NAME=quote(self._company_name)))]
            required_file_list += ['{bin_dir}/' + MEDIASERVER_DIST_FNAME,
                                   '{test_dir}/test_utils/' + BOX_PROVISION_MEDIASERVER]
        for script in provision_scripts or []:
            vm_commands += [make_vm_provision_command(script)]
            required_file_list.append('{test_dir}/' + script)
        return BoxConfig(name, ip_address_list, required_file_list, vm_commands, vbox_commands)


class BoxConfig(object):

    @classmethod
    def from_dict(cls, d, vm_name_prefix):
        return cls(
            name=d['name'],
            ip_address_list=map(IPNetwork, d['ip_address_list']),
            required_file_list=d['required_file_list'],
            vm_commands=[ConfigCommand.from_dict(command) for command in d['vm_commands']],
            vbox_commands=[ConfigCommand.from_dict(command) for command in d['vbox_commands']],
            idx=d['idx'],
            vm_name_prefix=vm_name_prefix,
            )

    def __init__(self, name, ip_address_list, required_file_list, vm_commands, vbox_commands, idx=None, vm_name_prefix=None):
        assert is_list_inst(ip_address_list, IPNetwork), repr(ip_address_list)
        self.name = name
        self.ip_address_list = ip_address_list
        self.required_file_list = required_file_list
        self.vm_commands = vm_commands
        self.vbox_commands = vbox_commands
        self.idx = idx
        self.vm_name_prefix = vm_name_prefix
        self.must_be_recreated = False  # this test requires fresh box
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
            vm_commands=[command.to_dict() for command in self.vm_commands],
            vbox_commands=[command.to_dict() for command in self.vbox_commands],
            idx=self.idx,
            )

    def matches(self, other):
        return (self.ip_address_list == other.ip_address_list
                and sorted(self.required_file_list) == sorted(other.required_file_list)
                and self.vm_commands == other.vm_commands
                and self.vbox_commands == other.vbox_commands)

    def box_name(self):
        assert self.idx is not None  # must be assigned for box_name
        if self.name:
            suffix = '-%s' % self.name
        else:
            suffix = ''
        return 'box-%d%s' % (self.idx, suffix)

    def vm_box_name(self):
        assert self.vm_name_prefix  # must be set before this call
        return self.vm_name_prefix + self.box_name()

    def rest_api_forwarded_port(self):
        return REST_API_FORWARDED_PORT_BASE + self.idx

    def get_vagrant_config_commands(self, type):
        return [command for command in self.vagrant_config_commands if command.type() == type]

    def expand(self, vbox_manage):
        network_commands = [
            self._expand_vm_command(make_vm_config_internal_network_command(vbox_manage, ip_address))
            for ip_address in self.ip_address_list]
        return ExpandedBoxConfig(
            box_name=self.box_name(),
            vm_box_name=self.vm_box_name(),
            rest_api_internal_port=MEDIASERVER_LISTEN_PORT,
            rest_api_forwarded_port=self.rest_api_forwarded_port(),
            vm_commands=network_commands + map(self._expand_vm_command, self.vm_commands),
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

    def __init__(self, box_name, vm_box_name, rest_api_internal_port, rest_api_forwarded_port, vm_commands, vbox_commands):
        self.box_name = box_name
        self.vm_box_name = vm_box_name
        self.rest_api_internal_port = rest_api_internal_port
        self.rest_api_forwarded_port = rest_api_forwarded_port
        self.vm_commands = vm_commands
        self.vbox_commands = vbox_commands
