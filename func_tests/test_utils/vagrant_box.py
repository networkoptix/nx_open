'''Vagrant wrappers classes'''

import os
import os.path
import logging
import re
import time
import shutil
import pytz
import jinja2
import vagrant
import vagrant.compat
from .host import RemoteSshHost
from .vbox_manage import VBoxManage
from .vagrant_box_config import DEFAULT_NATNET1


TEST_UTILS_DIR = os.path.abspath(os.path.dirname(__file__))

log = logging.getLogger(__name__)


def log_output(name, output):
    if not output:
        return  # do not log ''; skip None also
    if '\0' in output:
        log.debug('\t--> %s: %s bytes binary', name, len(output))
    elif len(output) > 200:
        log.debug('\t--> %s: %r...', name, output[:200])
    else:
        log.debug('\t--> %s: %r',  name, output.rstrip('\r\n'))


class RemotableVagrant(vagrant.Vagrant):

    def __init__(self, host, root=None, quiet_stdout=True, quiet_stderr=True):
        vagrant.Vagrant.__init__(self, root, quiet_stdout, quiet_stderr)
        self._host = host

    def _call_vagrant_command(self, args):
        self._run_vagrant_command(args)

    def _run_vagrant_command(self, args):
        cmd = ['vagrant'] + [arg for arg in args if arg is not None]
        return vagrant.compat.decode(
            self._host.run_command(cmd, cwd=self.root))


class Vagrant(object):

    def __init__(self, test_dir, vm_name_prefix, vm_host, bin_dir, vagrant_dir, vagrant_private_key_path, ssh_config_path):
        self._test_dir = test_dir
        self._vm_host = vm_host
        self._vbox_manage = VBoxManage(vm_name_prefix, vm_host)
        self._bin_dir = bin_dir
        self._vagrant_dir = vagrant_dir  # on vm_host
        self._vagrant_file_path = os.path.join(self._vagrant_dir, 'Vagrantfile')
        self._vagrant_private_key_path = vagrant_private_key_path  # may be None
        self._ssh_config_path = ssh_config_path
        self._vagrant = RemotableVagrant(
            self._vm_host, root=self._vagrant_dir, quiet_stdout=False, quiet_stderr=False)
        self.boxes = {}  # box name -> VagrantBox

    def destroy_all_boxes(self, boxes_config=None):
        if boxes_config is not None:
            log.info('Destroying old boxes: %s', ', '.join(config.vm_box_name for config in boxes_config))
            self._write_vagrantfile(boxes_config)
            self._vagrant.destroy()
        else:
            if os.path.exists(self._vagrant_file_path):
                log.info('Destroying old boxes using old Vagrantfile')
                self._vagrant.destroy()
            else:
                log.info('Old Vagrantfile "%s" is missing - will not destroy old boxes' % self._vagrant_file_path)
        if os.path.exists(self._ssh_config_path):
            os.remove(self._ssh_config_path)

    def init(self, boxes_config):
        self._write_vagrantfile(boxes_config)
        box2status = {status.name: status.state for status in self._vagrant.status()}
        for config in boxes_config:
            if not config.is_allocated:
                continue
            status = box2status[config.box_name]
            self._copy_required_files_to_vagrant_dir(config)
            if config.must_be_recreated and status != 'not created':
                self._vagrant.destroy(config.box_name)
                status = 'not created'
            if status != 'running':
                self._start_box(config)
            self._init_box(config)
        self._write_ssh_config([config.box_name for config in boxes_config
                                if config.is_allocated or box2status[config.box_name] == 'running'])
        for box in self.boxes.values():
            self._load_box_timezone(box)

    def _copy_required_files_to_vagrant_dir(self, box_config):
        for file_path_format in box_config.required_file_list:
            file_path = file_path_format.format(test_dir=self._test_dir, bin_dir=self._bin_dir)
            assert os.path.isfile(file_path), '%s is expected but is missing' % file_path
            self._vm_host.put_file(file_path, self._vagrant_dir)

    def _init_box(self, config):
        self.boxes[config.box_name] = VagrantBox(self, config)

    def _start_box(self, config):
        box_name = config.box_name
        log.info('Starting/creating box: %r, vm %r...', box_name, config.vm_box_name)
        self._cleanup_vms(config.vm_box_name)
        self._vagrant.up(vm_name=box_name)
        self._write_box_ssh_config(box_name)
        self.box_host(box_name, 'vagrant').run_command(['sudo', 'cp', '-r', '/home/vagrant/.ssh', '/root/'])

    def _cleanup_vms(self, vms_name):
        if not self._vbox_manage.does_vms_exist(vms_name): return
        if self._vbox_manage.get_vms_state(vms_name) not in ['poweroff', 'aborted']:
            self._vbox_manage.poweroff_vms(vms_name)
        self._vbox_manage.delete_vms(vms_name)

    def _load_box_timezone(self, box):
        tzname = self.box_host(box.name, 'root').run_command(['date', '+%Z'])
        timezone = pytz.timezone(tzname.rstrip())
        box.timezone = timezone

    def _write_vagrantfile(self, boxes_config):
        expanded_boxes_config_list = [config.expand(self._vbox_manage) for config in boxes_config]
        template_file_path = os.path.join(TEST_UTILS_DIR, 'Vagrantfile.jinja2')
        with open(template_file_path) as f:
            template = jinja2.Template(f.read())
        vagrantfile = template.render(
            natnet1=DEFAULT_NATNET1,
            template_file_path=template_file_path,
            boxes=expanded_boxes_config_list)
        self._vm_host.write_file(self._vagrant_file_path, vagrantfile)

    # write ssh config with single box in it
    def _write_box_ssh_config(self, box_name):
        with open(self._ssh_config_path, 'w') as f:
            f.write(self._vagrant.ssh_config(box_name))

    def _write_ssh_config(self, box_name_list):
        with open(self._ssh_config_path, 'w') as f:
            for box_name in box_name_list:
                f.write(self._vagrant.ssh_config(box_name))

    def box_host(self, box_name, user):
        return RemoteSshHost(box_name, user, self._vagrant_private_key_path, self._ssh_config_path, proxy_host=self._vm_host)


class VagrantBox(object):

    def __init__(self, vagrant, config):
        self._vagrant = vagrant
        self.config = config
        self.name = config.box_name
        self.timezone = None
        self.servers = []
        self.host = self._vagrant.box_host(self.name, 'root')

    def __str__(self):
        return self.name

    def __repr__(self):
        return 'Box(%s, timezone=%s)' % (self, self.timezone)
