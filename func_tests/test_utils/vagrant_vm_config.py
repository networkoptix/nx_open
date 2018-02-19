"""Configuration for vagrant VMs

Functional tests define configuration required for them indirectly (via 'vm' fixture) using VagrantVMConfig class.
"""

MEDIASERVER_LISTEN_PORT = 7001
SSH_PORT_OFFSET=2220


class VagrantVMConfigFactory(object):

    def __init__(self, customization_company_name):
        self._customization_company_name = customization_company_name

    def __call__(self, name=None, must_be_recreated=False, required_file_list=None):
        if not required_file_list:
            required_file_list = []
        return VagrantVMConfig(name, required_file_list, must_be_recreated)


class VagrantVMConfig(object):

    @classmethod
    def from_dict(cls, d):
        return cls(
            name=d['name'],
            required_file_list=d['required_file_list'],
            idx=d['idx'],
            vm_name_prefix=d['vm_name_prefix'],
            vm_port_base=d['vm_port_base'],
            )

    def __init__(self, name, required_file_list,
                 must_be_recreated=False,
                 idx=None, vm_name_prefix=None, vm_port_base=None):
        self.name = name
        self.required_file_list = required_file_list
        self.idx = idx
        self.vm_name_prefix = vm_name_prefix
        self.vm_port_base = vm_port_base
        self.must_be_recreated = must_be_recreated  # this test requires fresh VM
        self.is_allocated = False

    def __str__(self):
        return '%s, %r, %r' % (self.idx, self.name, self.required_file_list)

    def __repr__(self):
        return 'VagrantVMConfig(%s)' % self

    def to_dict(self):
        return dict(
            name=self.name,
            required_file_list=self.required_file_list,
            idx=self.idx,
            vm_name_prefix=self.vm_name_prefix,
            vm_port_base=self.vm_port_base,
            )

    def clone(self, idx, vm_name_prefix, vm_port_base):
        return VagrantVMConfig(
            name=self.name,
            required_file_list=self.required_file_list,
            idx=idx,
            vm_name_prefix=vm_name_prefix,
            vm_port_base=vm_port_base,
            )

    def matches(self, other):
        return sorted(self.required_file_list) == sorted(other.required_file_list)

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

    def expand(self):
        return ExpandedVagrantVMConfig(
            vagrant_name=self.vagrant_name,
            virtualbox_name=self.virtualbox_name,
            rest_api_internal_port=MEDIASERVER_LISTEN_PORT,
            rest_api_forwarded_port=self.rest_api_forwarded_port,
            ssh_forwarded_port=self.ssh_forwarded_port,
            )


class ExpandedVagrantVMConfig(object):

    def __init__(self, vagrant_name, virtualbox_name, rest_api_internal_port, rest_api_forwarded_port,
                 ssh_forwarded_port):
        self.vagrant_name = vagrant_name
        self.virtualbox_name = virtualbox_name
        self.rest_api_internal_port = rest_api_internal_port
        self.rest_api_forwarded_port = rest_api_forwarded_port
        self.ssh_forwarded_port = ssh_forwarded_port
