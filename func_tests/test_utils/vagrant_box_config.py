'''Configuration for vagrant boxes.

Functional tests define configuration required for them indirectly (via 'box' fixture) using BoxConfig class.
'''

import datetime
import pytz


DEFAULT_NATNET1 = '10.0.5/24'
DEFAULT_HOSTNET = '10.0.6.0'
BOX_PROVISION_MEDIASERVER = 'box-provision-mediaserver.sh'
MEDIASERVER_DIST_FNAME = 'mediaserver.deb'  # expected in vagrant dir


class ConfigCommand(object):

    @staticmethod
    def from_dict(d):
        type = d['type']
        if type == VirtualBoxConfigCommand.type():
            return VirtualBoxConfigCommand.from_dict(d)
        if type == VmConfigCommand.type():
            return VmConfigCommand.from_dict(d)
        assert False, repr(type)  # Unknown command type


class VirtualBoxConfigCommand(ConfigCommand):

    @classmethod
    def from_dict(cls, d):
        return cls(args=d['args'])

    def __init__(self, args):
        self.args = args  # str list

    def __eq__(self, other):
        if not isinstance(other, VirtualBoxConfigCommand):
            return false
        return self.args == other.args

    def __repr__(self):
        return str(self.args)

    def to_dict(self):
        return dict(
            type=self.type(),
            args=self.args)

    @staticmethod
    def type():
        return 'virtualbox'

    def expand(self, var_name):
        return '{var_name}.customize [{args}]'.format(var_name=var_name, args=', '.join(self.args))


class VmConfigCommand(ConfigCommand):

    @classmethod
    def from_dict(cls, d):
        return cls(function=d['function'],
                   args=d['args'],
                   kwargs=d['kwargs'])

    def __init__(self, function, args, kwargs):
        self.function = function  # str
        self.args = args          # str list
        self.kwargs = kwargs      # str->str dict

    def __eq__(self, other):
        if not isinstance(other, VmConfigCommand):
            return false
        return (self.function == other.function and
                self.args == other.args and
                self.kwargs == other.kwargs)

    def __repr__(self):
        return '%s %s %s' % (self.function, self.args, self.kwargs)

    def to_dict(self):
        return dict(
            type=self.type(),
            function=self.function,
            args=self.args,
            kwargs=self.kwargs)

    @staticmethod
    def type():
        return 'vm'

    def expand(self, var_name):
        return '{var_name}.vm.{function} {args}{kw_delimiter}{kwargs}'.format(
            var_name=var_name, function=self.function, args=', '.join(self.args),
            kw_delimiter=', ' if self.kwargs else '',
            kwargs=', '.join('%s: %s' % (name, value) for name, value in self.kwargs.items()))


def make_vm_config_private_network_command(ip_address):
    l = ip_address.split('.')
    assert len(l) == 4, 'Invalid ip address: %r' % ip_address
    kwargs = dict(ip='"%s"' % ip_address)
    if int(l[3]) == 0:  # address ending with 0 (1.2.3.0) means it is just a network, and dhcp must be used
        kwargs['type'] = ':dhcp'
    return VmConfigCommand('network', [':private_network'], kwargs)

def make_vm_provision_command(script, env=None):
    kwargs = dict(path='"%s"' % script)
    if env:
        kwargs['env'] = '{%s}' % ', '.join('%s: %s' % (name, value) for name, value in env.items())
    return VmConfigCommand('provision', [':shell'], kwargs)

def make_vbox_netnat_command(net_idx, network):
    return VirtualBoxConfigCommand([':modifyvm', ':id', '"--natnet%d"' % net_idx, '"%s"' % network])

def make_vbox_host_time_disabled_command():
    return VirtualBoxConfigCommand([':setextradata', ':id', '"VBoxInternal/Devices/VMMDev/0/Config/GetHostTimeDisabled"', '1'])


class BoxConfigFactory(object):

    def __init__(self, company_name):
        self._company_name = company_name

    # ip_address may end with .0 (like 1.2.3.0); this will be treated as network address, and dhcp will be used for it
    def __call__(self, name=None, install_server=True, provision_scripts=None,
                 ip_address_list=None, required_file_list=None, sync_time=True):
        commands = []
        if not required_file_list:
            required_file_list = []
        for ip_address in ip_address_list or [DEFAULT_HOSTNET]:
            commands += [make_vm_config_private_network_command(ip_address)]
        if not sync_time:
            commands += [make_vbox_host_time_disabled_command()]
        if install_server:
            commands += [make_vm_provision_command(
                BOX_PROVISION_MEDIASERVER, env=dict(COMPANY_NAME='"%s"' % self._company_name))]
            required_file_list += ['{bin_dir}/' + MEDIASERVER_DIST_FNAME,
                                   '{test_dir}/test_utils/' + BOX_PROVISION_MEDIASERVER]
        for script in provision_scripts or []:
            commands += [make_vm_provision_command(script)]
            required_file_list.append('{test_dir}/' + script)
        return BoxConfig(None, name, required_file_list, commands)


class BoxConfig(object):

    @classmethod
    def from_dict(cls, d, vm_name_prefix):
        timezone = d['timezone']
        return cls(idx=d['idx'],
                   name=d['name'],
                   required_file_list=d['required_file_list'],
                   vagrant_config_commands=[ConfigCommand.from_dict(command) for command in d['vagrant_config_commands']],
                   vm_name_prefix=vm_name_prefix,
                   timezone=pytz.timezone(timezone) if timezone else None)

    def __init__(self, idx, name, required_file_list, vagrant_config_commands, vm_name_prefix=None, timezone=None):
        assert timezone is None or isinstance(timezone, datetime.tzinfo), repr(timezone)
        self.idx = idx
        self.name = name
        self.required_file_list = required_file_list
        self.vagrant_config_commands = vagrant_config_commands
        self.vm_name_prefix = vm_name_prefix
        self.ip_address = None
        self.port = None
        self.timezone = timezone  # pytz.timezone or None
        self.must_be_recreated = False  # this test requires fresh box
        self.is_allocated = False

    def __str__(self):
        return '%s, %r, %r, %r' % (self.idx, self.name, self.required_file_list, self.vagrant_config_commands)

    def __repr__(self):
        return 'BoxConfig(%s)' % self

    def to_dict(self):
        return dict(idx=self.idx,
                    name=self.name,
                    required_file_list=self.required_file_list,
                    vagrant_config_commands=[command.to_dict() for command in self.vagrant_config_commands],
                    timezone=str(self.timezone) if self.timezone else None)

    def matches(self, box):
        return (sorted(self.required_file_list) == sorted(box.required_file_list) and
                self.vagrant_config_commands == box.vagrant_config_commands)

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

    def get_vagrant_config_commands(self, type):
        return [command for command in self.vagrant_config_commands if command.type() == type]
