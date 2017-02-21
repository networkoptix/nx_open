import os
import os.path
import logging
import re
import time
import subprocess
import shutil
import pytz
import jinja2
import vagrant
from vagrant_box_config import DEFAULT_NATNET1, DEFAULT_HOSTNET


TEST_DIR = os.path.abspath(os.path.dirname(__file__))

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


class Vagrant(object):

    def __init__(self, vagrant_dir, ssh_config_path):
        self._vagrant_dir = vagrant_dir
        self._ssh_config_path = ssh_config_path
        self._vagrant_log_fpath = os.path.join(vagrant_dir, 'vagrant.log')
        self._clear_vagrant_log()
        log_cm = vagrant.make_file_cm(self._vagrant_log_fpath)
        self._vagrant = vagrant.Vagrant(
            quiet_stdout=False, root=self._vagrant_dir, err_cm=log_cm,
            env=dict(os.environ, VAGRANT_NO_COLOR=''))
        self.boxes = {}  # box name -> VagrantBox

    def _clear_vagrant_log(self):
        with open(self._vagrant_log_fpath, 'w') as f:
            pass

    def _wrap_vagrant_call(self, fn, *args, **kw):
        try:
            return fn(*args, **kw)
        except subprocess.CalledProcessError as x:
            if x.output is None:  # vagrant uses check_call, it returns no output
                with open(self._vagrant_log_fpath) as f:
                    log.error('=== Vagrant log:  ===============>8 =======================')
                    log.error('%s', f.read())
                    log.error('================================= 8<=======================')
            raise

    def destroy_all_boxes(self, boxes_config):
        log.info('destroying old boxes: %s', ', '.join(config.vm_box_name() for config in boxes_config))
        self._write_vagrantfile(boxes_config)
        self._destroy_all_boxes()
        if os.path.exists(self._ssh_config_path):
            os.remove(self._ssh_config_path)

    # updates boxes_config: set ip_address and timezone
    def init(self, boxes_config):
        self._write_vagrantfile(boxes_config)
        box2status = {status.name: status.state for status in self._vagrant.status()}
        for config in boxes_config:
            if not config.is_allocated:
                continue
            for script in config.provision_scripts:
                shutil.copy2(os.path.join(TEST_DIR, script), self._vagrant_dir)
            if box2status[config.box_name()] != 'running':
                self._start_box(config.box_name())
            self._init_box(config)
        self._write_vagrantfile(boxes_config)  # now ip_address is known for just-started boxes - write it
        self._write_ssh_config(self.boxes.keys())  # update ssh config with new addresses
        for box in self.boxes.values():
            self._load_box_timezone(box)

    def _destroy_all_boxes(self):
        self._wrap_vagrant_call(self._vagrant.destroy)

    def _init_box(self, config):
        config.ip_address = self._load_box_ip_address(config)
        log.debug('IP address for %s: %s', config.box_name(), config.ip_address)
        self.boxes[config.box_name()] = VagrantBox(self, config)

    def _start_box(self, box_name):
        log.info('Starting/creating box: %r...', box_name)
        self._wrap_vagrant_call(self._vagrant.up, vm_name=box_name)
        self._write_box_ssh_config(box_name)
        self.run_ssh_command(box_name, 'vagrant', ['sudo', 'cp', '-r', '/home/vagrant/.ssh', '/root/'])

    def _load_box_ip_address(self, config):
        adapter_idx = 1  # use ip address from first host network
        output = subprocess.check_output([
            'VBoxManage', '--nologo', 'guestproperty', 'get',
            config.vm_box_name(), '/VirtualBox/GuestInfo/Net/%d/V4/IP' % adapter_idx])
        l = output.strip().split()
        assert l[0] == 'Value:', repr(output)  # Does interface exist?
        return l[1]

    def _load_box_timezone(self, box):
        tzname = self.run_ssh_command(box.name, 'root', ['date', '+%Z'])
        timezone = pytz.timezone(tzname.rstrip())
        box.timezone = box.config.timezone = timezone

    def _write_vagrantfile(self, boxes_config):
        template_fpath = os.path.join(TEST_DIR, 'Vagrantfile.jinja2')
        with open(template_fpath) as f:
            template = jinja2.Template(f.read())
        vagrantfile = template.render(
            natnet1=DEFAULT_NATNET1,
            template_fpath=template_fpath,
            boxes=boxes_config)
        with open(os.path.join(self._vagrant_dir, 'Vagrantfile'), 'w') as f:
            f.write(vagrantfile)

    def _write_box_ssh_config(self, box_name):
        with open(self._ssh_config_path, 'a') as f:
            f.write(self._vagrant.ssh_config(box_name))

    def _write_ssh_config(self, box_name_list):
        with open(self._ssh_config_path, 'w') as f:
            for box_name in box_name_list:
                f.write(self._vagrant.ssh_config(box_name))

    def make_sudo_command_args(self, box_name, user, args):
        return ['ssh', '-F', self._ssh_config_path, '%s@%s' % (user, box_name)] + list(args)

    def run_ssh_command(self, box_name, user, args):
        args = self.make_sudo_command_args(box_name, user, args)
        log.debug('ssh: %s', ' '.join(args))
        output = subprocess.check_output(args, stderr=subprocess.STDOUT)
        log_output('stdout', output)
        return output


class VagrantBox(object):

    def __init__(self, vagrant, config):
        self._vagrant = vagrant
        self.config = config
        self.name = config.box_name()
        self.ip_address = config.ip_address  # str
        self.timezone = None
        self.servers = []

    def __str__(self):
        return '%s@%s/%s' % (self.name, self.ip_address, self.timezone)

    def __repr__(self):
        return 'Box%s' % self

    def _make_sudo_command_args(self, args, user=None):
        return self._vagrant.make_sudo_command_args(self.name, user or 'root', args)

    def run_ssh_command(self, args, user=None):
        return self._vagrant.run_ssh_command(self.name, user or 'root', args)

    def run_ssh_command_with_input(self, args, input, user=None):
        args = self._make_sudo_command_args(args, user)
        log.debug('ssh: %s (with %s bytes input)', ' '.join(args), len(input))
        pipe = subprocess.Popen(
            args,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            stdin=subprocess.PIPE)
        out, err = pipe.communicate(input)
        log_output('stdout', out)
        log_output('stderr', err)
        return out

    def get_file(self, path):
        return self.run_ssh_command(['cat'] + [path])

    def put_file(self, path, contents):
        dir = os.path.dirname(path)
        self.run_ssh_command(['mkdir', '-p', dir])
        return self.run_ssh_command_with_input(['cat', '>', path], contents)

    def change_host_name(self, host_name):
        self.run_ssh_command(['hostnamectl', 'set-hostname', host_name])
        file = self.get_file('/etc/hosts')
        lines = [line for line in file.splitlines() if not line.startswith('127.')]
        lines = ['127.0.0.1\tlocalhost %s' % host_name] + lines
        self.put_file('/etc/hosts', '\n'.join(lines) + '\n')
