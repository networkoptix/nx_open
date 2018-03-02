import logging

import jinja2
import vagrant
import vagrant.compat
from pathlib2 import Path
from pylru import lrudecorator

from test_utils.networking import NodeNetworking, reset_networking
from test_utils.networking.linux import LinuxNodeNetworking
from test_utils.networking.virtual_box import VirtualBoxNodeNetworking
from test_utils.os_access import LocalAccess
from test_utils.ssh.sshd import optimize_sshd
from .os_access import ProcessError, SshAccess
from .vagrant_vm_config import VagrantVMConfig
from .virtualbox_management import VirtualboxManagement

log = logging.getLogger(__name__)

TEST_UTILS_DIR = Path(__file__).parent.resolve()


class RemotableVagrant(vagrant.Vagrant):
    def __init__(self, host, root=None, quiet_stdout=True, quiet_stderr=True):
        vagrant.Vagrant.__init__(self, root, quiet_stdout, quiet_stderr)
        self._os_access = host

    def _call_vagrant_command(self, args):
        self._run_vagrant_command(args)

    def _run_vagrant_command(self, args):
        cmd = ['vagrant'] + [arg for arg in args if arg is not None]
        return vagrant.compat.decode(
            self._os_access.run_command(cmd, cwd=self.root))


class VagrantVMFactory(object):
    _vagrant_vms_cache_key = 'nx/vagrant_vms'

    def __init__(self, cache, ssh_config, vm_host, bin_dir, work_dir, config_factory):
        self._ssh_config = ssh_config
        self._cache = cache
        self._bin_dir = bin_dir
        self._config_factory = config_factory
        self._vm_name_prefix = vm_host.vm_name_prefix
        self._vm_port_base = vm_host.vm_port_base
        if vm_host.hostname:
            self.vm_host_hostname = vm_host.hostname
            self._host_os_access = SshAccess(self._ssh_config.path, vm_host.hostname)
            self._vagrant_dir = vm_host.work_dir / 'vagrant'
            self._vagrant_private_key_path = work_dir / 'vagrant_insecure_private_key'
            self._host_os_access.get_file('.vagrant.d/insecure_private_key', self._vagrant_private_key_path)
        else:
            self.vm_host_hostname = 'localhost'
            self._host_os_access = LocalAccess()
            self._vagrant_dir = work_dir / 'vagrant'
            self._vagrant_private_key_path = Path().home() / '.vagrant.d' / 'insecure_private_key'
        self._virtualbox_vm = VirtualboxManagement(self._host_os_access)
        self._vagrant_file_path = self._vagrant_dir / 'Vagrantfile'
        self._vagrant = RemotableVagrant(
            self._host_os_access, root=str(self._vagrant_dir), quiet_stdout=False, quiet_stderr=False)
        self._vms = self._discover_existing_vms()  # VM name -> VagrantVM
        self._allocated_vms = {}
        self._init_running_vms()
        if self._vms:
            self._last_vm_idx = max(vm.config.idx for vm in self._vms.values())
        else:
            self._last_vm_idx = 0

    def _discover_existing_vms(self):
        vm_config_list = self._load_vms_config_from_cache()
        self._write_vagrantfile(vm_config_list)
        name2running = {status.name: status.state == 'running' for status in self._vagrant.status()}
        return {config.vagrant_name: self._make_vagrant_vm(config, is_running=name2running.get(config.vagrant_name, False))
                for config in vm_config_list}

    def _load_vms_config_from_cache(self):
        return [VagrantVMConfig.from_dict(d) for d in self._cache.get(self._vagrant_vms_cache_key, [])]

    def _save_vms_config_to_cache(self):
        self._cache.set(self._vagrant_vms_cache_key, [vm.config.to_dict() for vm in self._vms.values()])

    def _make_vagrant_vm(self, config, is_running):
        self._ssh_config.add_host(
            self._host_os_access.hostname,
            alias=config.vagrant_name,
            port=config.ssh_forwarded_port,
            user=u'vagrant',
            key_path=self._vagrant_private_key_path)
        return VagrantVM(
            self._bin_dir,
            self._vagrant_dir,
            self._vagrant,
            self._ssh_config.path,
            self._host_os_access,
            config,
            is_running)

    def _init_running_vms(self):
        for vm in self._vms.values():
            if vm.is_running:
                vm.init(safe=True)

    def destroy_all(self):
        if not self._vms:
            return
        self._vagrant.destroy()
        self._vms.clear()
        self._save_vms_config_to_cache()
        self._last_vm_idx = 0
        self._write_vagrantfile([b.config for b in self._vms.values()])

    def release_all_vms(self):
        log.debug('Releasing all VMs: %s', ', '.join(vm.vagrant_name for vm in self._vms.values() if vm.is_allocated))
        self.get.clear()
        for vm in self._vms.values():
            vm.is_allocated = False

    @lrudecorator(1000)
    def get(self, name=None):
        found_vm = None
        for vm in self._vms.values():
            if not vm.is_allocated and vm.is_running:
                found_vm = vm
                break
        if found_vm is None:
            for vm in self._vms.values():
                if not vm.is_allocated:
                    found_vm = vm
                    break
            if found_vm is None:
                found_vm = self._create_vm(self._config_factory(name))
            found_vm.start()
        found_vm.init()
        found_vm.is_allocated = True
        return found_vm

    def _remove_vms(self, vms_name):
        log.info('Removing old vm %s...', vms_name)
        if self._virtualbox_vm.get_vms_state(vms_name) not in ['poweroff', 'aborted']:
            self._virtualbox_vm.poweroff_vms(vms_name)
        self._virtualbox_vm.delete_vms(vms_name)

    def _create_vm(self, config_template):
        self._last_vm_idx += 1
        config = config_template.clone(
            idx=self._last_vm_idx,
            vm_name_prefix=self._vm_name_prefix,
            vm_port_base=self._vm_port_base,
            )
        vm = self._make_vagrant_vm(config, is_running=False)
        self._vms[vm.vagrant_name] = vm
        self._save_vms_config_to_cache()
        self._write_vagrantfile([b.config for b in self._vms.values()])
        return vm

    def _write_vagrantfile(self, vm_config_list):
        expanded_vms_config_list = [
            config.expand() for config in vm_config_list]
        template_file_path = TEST_UTILS_DIR / 'Vagrantfile.jinja2'
        template = jinja2.Template(template_file_path.read_text())
        vagrantfile = template.render(
            template_file_path=template_file_path,
            vms=expanded_vms_config_list)
        self._host_os_access.write_file(self._vagrant_file_path, vagrantfile.encode())


class VagrantVM(object):
    def __init__(self, bin_dir, vagrant_dir, vagrant, _ssh_config_path, host_os_access, config, is_running):
        self._bin_dir = bin_dir
        self._vagrant_dir = vagrant_dir
        self._vagrant = vagrant
        self.config = config
        self.host_os_access = host_os_access
        self.os_access = None  # Until VM is started.
        self.timezone = None
        self.is_allocated = False
        self.is_running = is_running
        self._ssh_config_path = _ssh_config_path
        self.networking = None  # TODO: Move initialization from init() method.

    def __str__(self):
        return '%s virtualbox_name=%s timezone=%s' % (self.vagrant_name, self.virtualbox_name, self.timezone or '?')

    def __repr__(self):
        return '<%s>' % self

    @property
    def vagrant_name(self):
        return self.config.vagrant_name

    @property
    def virtualbox_name(self):
        return self.config.virtualbox_name

    def init(self, safe=False):
        assert self.is_running
        try:
            os_access = SshAccess(self._ssh_config_path, self.vagrant_name).become('root')
            self.timezone = os_access.get_timezone()  # and check it is accessible
        except ProcessError as x:
            if safe and 'Permission denied' in x.output:
                log.info('Unable to access machine %s as root, will reinit it',
                         self.vagrant_name)  # .ssh copying to root was missing?
                self.is_running = False
            else:
                raise
        else:
            self.os_access = os_access
        self.networking = NodeNetworking(
            LinuxNodeNetworking(self.os_access),
            VirtualBoxNodeNetworking(self.virtualbox_name))
        reset_networking(self)

    def start(self):
        assert not self.is_running
        log.info('Starting VM: %s...', self)
        self._vagrant.up(vm_name=self.vagrant_name)
        self.os_access = SshAccess(self._ssh_config_path, self.vagrant_name).become('root')
        optimize_sshd(self.os_access)
        self.is_running = True

    def destroy(self):
        assert self.is_running
        self._vagrant.destroy(self.vagrant_name)
        self.is_running = False
