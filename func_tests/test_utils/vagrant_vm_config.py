"""Configuration for vagrant VMs

Functional tests define configuration required for them indirectly (via 'vm' fixture) using VagrantVMConfig class.
"""

from netaddr import IPNetwork

from .utils import is_list_inst, quote

DEFAULT_NATNET1 = '10.10.0/24'
DEFAULT_PRIVATE_NET = '10.10.1/24'
MEDIASERVER_LISTEN_PORT = 7001
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


def make_vm_provision_command(script, args=None, env=None):
    kwargs = dict(path=quote(script))
    if args:
        kwargs['args'] = '[%s]' % ', '.join(quote(arg) for arg in args)
    if env:
        kwargs['env'] = '{%s}' % ', '.join('%s: %s' % (name, value) for name, value in env.items())
    return ConfigCommand('provision', [':shell'], kwargs)

def make_virtualbox_host_time_disabled_command():
    return ConfigCommand(None, [':setextradata', ':id', quote('VBoxInternal/Devices/VMMDev/0/Config/GetHostTimeDisabled'), '1'])


class VagrantVMConfigFactory(object):

    def __init__(self, customization_company_name):
        self._customization_company_name = customization_company_name

    # ip_address may end with .0 (like 1.2.3.0); this will be treated as network address, and dhcp will be used for it
    def __call__(self, name=None, provision_scripts=None, must_be_recreated=False,
                 ip_address_list=None, required_file_list=None, sync_time=True):
        vagrant_conf = []
        virtualbox_conf = []
        ip_address_list = map(IPNetwork, ip_address_list or [DEFAULT_PRIVATE_NET])
        if not required_file_list:
            required_file_list = []
        if not sync_time:
            virtualbox_conf += [make_virtualbox_host_time_disabled_command()]
        for script in provision_scripts or []:
            vagrant_conf += [make_vm_provision_command(script)]
            required_file_list.append('{test_dir}/' + script)
        return VagrantVMConfig(name, ip_address_list, required_file_list,
                               vagrant_conf, virtualbox_conf, must_be_recreated)


class VagrantVMConfig(object):

    @classmethod
    def from_dict(cls, d):
        return cls(
            name=d['name'],
            ip_address_list=map(IPNetwork, d['ip_address_list']),
            required_file_list=d['required_file_list'],
            vagrant_conf=[ConfigCommand.from_dict(command) for command in d['vagrant_conf']],
            virtualbox_conf=[ConfigCommand.from_dict(command) for command in d['virtualbox_conf']],
            idx=d['idx'],
            vm_name_prefix=d['vm_name_prefix'],
            vm_port_base=d['vm_port_base'],
            )

    def __init__(self, name, ip_address_list, required_file_list,
                 vagrant_conf, virtualbox_conf, must_be_recreated=False,
                 idx=None, vm_name_prefix=None, vm_port_base=None):
        assert is_list_inst(ip_address_list, IPNetwork), repr(ip_address_list)
        self.name = name
        self.ip_address_list = ip_address_list
        self.required_file_list = required_file_list
        self.vagrant_conf = vagrant_conf
        self.virtualbox_conf = virtualbox_conf
        self.idx = idx
        self.vm_name_prefix = vm_name_prefix
        self.vm_port_base = vm_port_base
        self.must_be_recreated = must_be_recreated  # this test requires fresh VM
        self.is_allocated = False

    def __str__(self):
        return '%s, %r, %r, %r, %r' % (self.idx, self.name, self.required_file_list, self.vagrant_conf, self.virtualbox_conf)

    def __repr__(self):
        return 'VagrantVMConfig(%s)' % self

    def to_dict(self):
        return dict(
            name=self.name,
            ip_address_list=map(str, self.ip_address_list),
            required_file_list=self.required_file_list,
            vagrant_conf=[command.to_dict() for command in self.vagrant_conf],
            virtualbox_conf=[command.to_dict() for command in self.virtualbox_conf],
            idx=self.idx,
            vm_name_prefix=self.vm_name_prefix,
            vm_port_base=self.vm_port_base,
            )

    def clone(self, idx, vm_name_prefix, vm_port_base):
        return VagrantVMConfig(
            name=self.name,
            ip_address_list=self.ip_address_list,
            required_file_list=self.required_file_list,
            vagrant_conf=self.vagrant_conf,
            virtualbox_conf=self.virtualbox_conf,
            idx=idx,
            vm_name_prefix=vm_name_prefix,
            vm_port_base=vm_port_base,
            )

    def matches(self, other):
        return (self.name == other.name
                and self.ip_address_list == other.ip_address_list
                and sorted(self.required_file_list) == sorted(other.required_file_list)
                and self.vagrant_conf == other.vagrant_conf
                and self.virtualbox_conf == other.virtualbox_conf)

    @property
    def vagrant_name(self):
        assert self.idx is not None  # must be assigned for vagrant_name
        if self.name:
            suffix = '-%s' % self.name
        else:
            suffix = ''
        return 'vm-%d%s' % (self.idx, suffix)

    @property
    def virtualbox_name(self):
        assert self.vm_name_prefix  # must be set before this call
        return self.vm_name_prefix + self.vagrant_name

    @property
    def rest_api_forwarded_port(self):
        return self.vm_port_base + self.idx

    @property
    def ssh_forwarded_port(self):
        return self.vm_port_base + SSH_PORT_OFFSET + self.idx

    def expand(self, virtualbox_management):
        return ExpandedVagrantVMConfig(
            vagrant_name=self.vagrant_name,
            virtualbox_name=self.virtualbox_name,
            rest_api_internal_port=MEDIASERVER_LISTEN_PORT,
            rest_api_forwarded_port=self.rest_api_forwarded_port,
            ssh_forwarded_port=self.ssh_forwarded_port,
            vagrant_conf=map(self._expand_vm_command, self.vagrant_conf),
            virtualbox_conf=map(self._expand_virtualbox_command, self.virtualbox_conf),
            )

    def _expand_vm_command(self, command):
        return 'vm.{function} {args}{kw_delimiter}{kwargs}'.format(
            function=command.function, args=', '.join(command.args),
            kw_delimiter=', ' if command.kwargs else '',
            kwargs=', '.join('%s: %s' % (name, value) for name, value in command.kwargs.items()))

    def _expand_virtualbox_command(self, command):
        return 'customize [{args}]'.format(args=', '.join(command.args))


class ExpandedVagrantVMConfig(object):

    def __init__(self, vagrant_name, virtualbox_name, rest_api_internal_port, rest_api_forwarded_port,
                 ssh_forwarded_port, vagrant_conf, virtualbox_conf):
        self.vagrant_name = vagrant_name
        self.virtualbox_name = virtualbox_name
        self.rest_api_internal_port = rest_api_internal_port
        self.rest_api_forwarded_port = rest_api_forwarded_port
        self.ssh_forwarded_port = ssh_forwarded_port
        self.vagrant_conf = vagrant_conf
        self.virtualbox_conf = virtualbox_conf
