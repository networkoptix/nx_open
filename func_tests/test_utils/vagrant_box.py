import logging
import socket
import tempfile

import jinja2
import vagrant
import vagrant.compat
from pathlib2 import Path

from .os_access import ProcessError, SshAccess, host_from_config
from .vagrant_box_config import DEFAULT_NATNET1, BoxConfig
from .vbox_manage import VBoxManage

log = logging.getLogger(__name__)

TEST_UTILS_DIR = Path(__file__).parent.resolve()


def log_output(name, output):
    if not output:
        return  # do not log ''; skip None also
    if '\0' in output:
        log.debug('\t--> %s: %s bytes binary', name, len(output))
    elif len(output) > 200:
        log.debug('\t--> %s: %r...', name, output[:200])
    else:
        log.debug('\t--> %s: %r', name, output.rstrip('\r\n'))


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


class VagrantBoxFactory(object):
    _vagrant_boxes_cache_key = 'nx/vagrant_boxes'

    def __init__(self, cache, options, config_factory):
        self._cache = cache
        self._bin_dir = options.bin_dir
        self._vagrant_private_key_path = None  # defined for remote ssh vm host
        self._config_factory = config_factory
        self._host_os_access = host_from_config(options.vm_ssh_host_config)
        self._vm_name_prefix = options.vm_name_prefix
        self._vm_port_base = options.vm_port_base
        self._vbox_manage = VBoxManage(self._vm_name_prefix, self._host_os_access)
        if options.vm_ssh_host_config:
            self._vagrant_dir = options.vm_host_work_dir / 'vagrant'
            self._vagrant_private_key_path = options.work_dir / 'vagrant_insecure_private_key'
            self._copy_vagrant_insecure_ssh_key(to_local_path=self._vagrant_private_key_path)
        else:
            self._vagrant_dir = options.work_dir / 'vagrant'
        self._vagrant_file_path = self._vagrant_dir / 'Vagrantfile'
        self._ssh_config_path = options.work_dir / 'ssh.config'
        self._existing_vms_list = set(self._vbox_manage.get_vms_list())
        options.work_dir.mkdir(parents=True, exist_ok=True)
        self._vagrant = RemotableVagrant(
            self._host_os_access, root=str(self._vagrant_dir), quiet_stdout=False, quiet_stderr=False)
        self._boxes = self._discover_existing_boxes()  # box name -> VagrantVirtualMachine
        self._write_ssh_config()
        self._init_running_boxes()
        if self._boxes:
            self._last_box_idx = max(box.config.idx for box in self._boxes.values())
        else:
            self._last_box_idx = 0

    def _copy_vagrant_insecure_ssh_key(self, to_local_path):
        log.debug('picking vagrant insecure ssh key:')
        self._host_os_access.get_file('.vagrant.d/insecure_private_key', to_local_path)

    def _discover_existing_boxes(self):
        box_config_list = self._load_boxes_config_from_cache()
        self._write_vagrantfile(box_config_list)
        name2running = {status.name: status.state == 'running' for status in self._vagrant.status()}
        return {config.box_name: self._make_vagrant_box(config, is_running=name2running.get(config.box_name, False))
                for config in box_config_list}

    def _load_boxes_config_from_cache(self):
        return [BoxConfig.from_dict(d) for d in self._cache.get(self._vagrant_boxes_cache_key, [])]

    def _save_boxes_config_to_cache(self):
        self._cache.set(self._vagrant_boxes_cache_key, [box.config.to_dict() for box in self._boxes.values()])

    def _make_vagrant_box(self, config, is_running):
        return VagrantVirtualMachine(
            self._bin_dir,
            self._vagrant_dir,
            self._vagrant,
            self._vagrant_private_key_path,
            self._ssh_config_path,
            self._host_os_access,
            config,
            is_running)

    def _init_running_boxes(self):
        for box in self._boxes.values():
            if box.is_running:
                box.init(safe=True)

    def destroy_all(self):
        if not self._boxes:
            return
        self._vagrant.destroy()
        self._boxes.clear()
        self._save_boxes_config_to_cache()
        self._last_box_idx = 0
        self._existing_vms_list = set(self._vbox_manage.get_vms_list())

    def __call__(self, *args, **kw):
        config = self._config_factory(*args, **kw)
        return self._allocate_box(config)

    def release_all_boxes(self):
        log.debug('Releasing all boxes: %s', ', '.join(box.name for box in self._boxes.values() if box.is_allocated))
        for box in self._boxes.values():
            box.is_allocated = False

    def _allocate_box(self, config):
        box = self._find_matching_box(config)
        if not box:
            box = self._create_box(config)
        box.is_allocated = True
        if config.must_be_recreated and box.is_running:
            box.destroy()
            self._existing_vms_list.remove(box.vm_name)
        if not box.is_running and box.vm_name in self._existing_vms_list:
            self._remove_vms(box.vm_name)
        if not box.is_running:
            box.start()
            self._existing_vms_list.add(box.vm_name)
            self._write_ssh_config()  # with newly added box; box must be already running for this
            box.init()
        log.info('BOX: %s', box)
        return box

    def _remove_vms(self, vms_name):
        log.info('Removing old vm %s...', vms_name)
        if self._vbox_manage.get_vms_state(vms_name) not in ['poweroff', 'aborted']:
            self._vbox_manage.poweroff_vms(vms_name)
        self._vbox_manage.delete_vms(vms_name)

    def _create_box(self, config_template):
        self._last_box_idx += 1
        config = config_template.clone(
            idx=self._last_box_idx,
            vm_name_prefix=self._vm_name_prefix,
            vm_port_base=self._vm_port_base,
            )
        box = self._make_vagrant_box(config, is_running=False)
        self._boxes[box.name] = box
        self._save_boxes_config_to_cache()
        self._write_vagrantfile([b.config for b in self._boxes.values()])
        return box

    def _find_matching_box(self, config_template):
        def find(pred=None):
            for name, box in sorted(self._boxes.items(), key=lambda (name, box): box.config.idx):
                if box.is_allocated:
                    continue
                if not box.config.matches(config_template):
                    continue
                if config_template.must_be_recreated:
                    return box
                if pred and not pred(box):
                    continue
                return box
            return None

        # first try running ones
        return find(lambda box: box.is_running) or find()

    def _write_vagrantfile(self, box_config_list):
        expanded_boxes_config_list = [
            config.expand(self._vbox_manage) for config in box_config_list]
        template_file_path = TEST_UTILS_DIR / 'Vagrantfile.jinja2'
        template = jinja2.Template(template_file_path.read_text())
        vagrantfile = template.render(
            natnet1=DEFAULT_NATNET1,
            template_file_path=template_file_path,
            boxes=expanded_boxes_config_list)
        self._host_os_access.write_file(self._vagrant_file_path, vagrantfile.encode())

    def _write_ssh_config(self):
        with self._ssh_config_path.open('w') as f:
            for box in self._boxes.values():
                if box.is_running:
                    f.write(self._vagrant.ssh_config(box.name).decode())


class VagrantVirtualMachine(object):
    def __init__(self, bin_dir, vagrant_dir, vagrant, vagrant_private_key_path,
                 ssh_config_path, host_os_access, config, is_running):
        self._bin_dir = bin_dir
        self._vagrant_dir = vagrant_dir
        self._vagrant = vagrant
        self._vagrant_private_key_path = vagrant_private_key_path
        self._ssh_config_path = ssh_config_path
        self.config = config
        self.host_os_access = host_os_access
        self.guest_os_access = None
        self.timezone = None
        self.is_allocated = False
        self.is_running = is_running

    def __str__(self):
        return '%s vm_name=%s timezone=%s' % (self.name, self.vm_name, self.timezone or '?')

    def __repr__(self):
        return '<%s>' % self

    @property
    def name(self):
        return self.config.box_name

    @property
    def vm_name(self):
        return self.config.vm_box_name

    def init(self, safe=False):
        assert self.is_running
        try:
            self.guest_os_access = self._make_os_access('root')
            self.timezone = self._load_timezone()  # and check it is accessible
        except ProcessError as x:
            if safe and 'Permission denied' in x.output:
                log.info('Unable to access machine %s as root, will reinit it',
                         self.name)  # .ssh copying to root was missing?
                self.guest_os_access = None
                self.is_running = False
            else:
                raise

    def start(self):
        assert not self.is_running
        log.info('Starting box: %s...', self)
        self._copy_required_files_to_vagrant_dir()
        self._vagrant.up(vm_name=self.name)
        with tempfile.NamedTemporaryFile() as f:
            f.write(self._vagrant.ssh_config(self.name))
            f.flush()
            self._make_os_access('vagrant', f.name).run_command(['sudo', 'cp', '-r', '/home/vagrant/.ssh', '/root/'])
            self._patch_sshd_config(self._make_os_access('root', f.name))
        self.is_running = True

    def destroy(self):
        assert self.is_running
        self._vagrant.destroy(self.name)
        self.is_running = False

    def _make_os_access(self, user, ssh_config_path=None):
        return SshAccess(self.name, user=user, key_path=self._vagrant_private_key_path,
                         config_path=ssh_config_path or self._ssh_config_path,
                         proxy_os_access=self.host_os_access)

    def _load_timezone(self):
        return self.guest_os_access.get_timezone()

    def _copy_required_files_to_vagrant_dir(self):
        test_dir = TEST_UTILS_DIR.parent
        for file_path_format in self.config.required_file_list:
            file_path = file_path_format.format(test_dir=test_dir, bin_dir=self._bin_dir)
            assert file_path.is_file(), '%s is expected but is missing' % file_path
            self.host_os_access.put_file(file_path, self._vagrant_dir)

    @staticmethod
    def _patch_sshd_config(root_ssh_host):
        """With default settings, connection takes ~1.5 sec."""
        sshd_config_path = '/etc/ssh/sshd_config'
        settings = root_ssh_host.read_file(sshd_config_path).split('\n')  # Preserve new line at the end!
        old_setting = 'UsePAM yes'
        new_setting = 'UsePAM no'
        try:
            old_setting_line_index = settings.index(old_setting)
        except ValueError:
            assert False, "Wanted to replace %s with %s but couldn't fine latter" % (old_setting, new_setting)
        settings[old_setting_line_index] = new_setting
        root_ssh_host.write_file(sshd_config_path, '\n'.join(settings))
        root_ssh_host.run_command(['service', 'ssh', 'reload'])


def port_is_accessible_from_this_machine(this_machine_port):
    try:
        socket.socket(('localhost', this_machine_port))
    except socket.error:
        return False
    else:
        return True
