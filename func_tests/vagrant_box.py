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
from host import RemoteSshHost
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

    def __init__(self, vm_host, bin_dir, vagrant_dir, vagrant_private_key_path, ssh_config_path):
        self._vm_host = vm_host
        self._bin_dir = bin_dir
        self._vagrant_dir = vagrant_dir  # on vm_host
        self._vagrant_private_key_path = vagrant_private_key_path  # may be None
        self._ssh_config_path = ssh_config_path
        self._vagrant = RemotableVagrant(
            self._vm_host, root=self._vagrant_dir, quiet_stdout=False, quiet_stderr=False)
        self.boxes = {}  # box name -> VagrantBox

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
            self._copy_required_files_to_vagrant_dir(config)
            if box2status[config.box_name()] != 'running':
                self._start_box(config)
            self._init_box(config)
        self._write_vagrantfile(boxes_config)  # now ip_address is known for just-started boxes - write it
        self._write_ssh_config(self.boxes.keys())  # write ssh config for all boxes, with new addresses
        for box in self.boxes.values():
            self._load_box_timezone(box)

    def _destroy_all_boxes(self):
        self._vagrant.destroy()

    def _copy_required_files_to_vagrant_dir(self, box_config):
        for file_path_format in box_config.required_file_list:
            file_path = file_path_format.format(test_dir=TEST_DIR, bin_dir=self._bin_dir)
            assert os.path.isfile(file_path), '%s is expected but is missing' % file_path
            self._vm_host.put_file(file_path, self._vagrant_dir)

    def _init_box(self, config):
        config.ip_address = self._load_box_ip_address(config)
        config.port = 22  # now we can connect directly, not thru port forwarded by vagrant
        log.info('IP address for %s: %s', config.box_name(), config.ip_address)
        self.boxes[config.box_name()] = VagrantBox(self, config)

    def _start_box(self, config):
        box_name = config.box_name()
        log.info('Starting/creating box: %r, vm %r...', box_name, config.vm_box_name())
        self._vagrant.up(vm_name=box_name)
        self._write_box_ssh_config(box_name)
        self.box_host(box_name, 'vagrant').run_command(['sudo', 'cp', '-r', '/home/vagrant/.ssh', '/root/'])

    def _load_box_ip_address(self, config):
        self._vm_host.run_command(['VBoxManage', '--nologo', 'list', 'vms'])
        self._vm_host.run_command(['VBoxManage', '--nologo', 'list', 'hostonlyifs'])
        self._vm_host.run_command(['VBoxManage', '--nologo', 'list', 'natnets'])
        self._vm_host.run_command(['VBoxManage', '--nologo', 'list', 'intnets'])
        self._vm_host.run_command(['VBoxManage', '--nologo', 'list', 'bridgedifs'])
        self._vm_host.run_command(['VBoxManage', '--nologo', 'list', 'dhcpservers'])
        self._vm_host.run_command(['/sbin/ifconfig'])
        adapter_idx = 1  # use ip address from first host network
        cmd = ['VBoxManage', '--nologo', 'guestproperty', 'get',
               config.vm_box_name(), '/VirtualBox/GuestInfo/Net/%d/V4/IP' % adapter_idx]
        # temporary: vbox returns invalid address sometimes; try load ip address several times with delay to see what happens
        for i in range(10):
            output = self._vm_host.run_command(cmd)
            time.sleep(0.5)
        l = output.strip().split()
        assert l[0] == 'Value:', repr(output)  # Does interface exist?
        return l[1]

    def _load_box_timezone(self, box):
        tzname = self.box_host(box.name, 'root').run_command(['date', '+%Z'])
        timezone = pytz.timezone(tzname.rstrip())
        box.timezone = box.config.timezone = timezone

    def _write_vagrantfile(self, boxes_config):
        template_file_path = os.path.join(TEST_DIR, 'Vagrantfile.jinja2')
        with open(template_file_path) as f:
            template = jinja2.Template(f.read())
        vagrantfile = template.render(
            natnet1=DEFAULT_NATNET1,
            template_file_path=template_file_path,
            boxes=boxes_config)
        self._vm_host.write_file(os.path.join(self._vagrant_dir, 'Vagrantfile'), vagrantfile)

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
        self.name = config.box_name()
        self.ip_address = config.ip_address  # str
        self.timezone = None
        self.servers = []
        self.host = self._vagrant.box_host(self.name, 'root')

    def __str__(self):
        return '%s@%s/%s' % (self.name, self.ip_address, self.timezone)

    def __repr__(self):
        return 'Box%s' % self

    def change_host_name(self, host_name):
        self.host.run_command(['hostnamectl', 'set-hostname', host_name])
        file = self.host.read_file('/etc/hosts')
        lines = [line for line in file.splitlines() if not line.startswith('127.')]
        lines = ['127.0.0.1\tlocalhost %s' % host_name] + lines
        self.host.write_file('/etc/hosts', '\n'.join(lines) + '\n')
