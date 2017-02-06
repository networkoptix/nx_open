import os
import os.path
import re
import StringIO
import subprocess
import contextlib
import pytz
import jinja2
import vagrant


TEST_DIR = os.path.abspath(os.path.dirname(__file__))


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
        self._vagrant_stdout = StringIO.StringIO()

    @contextlib.contextmanager
    def _capture_out_cm(self):
        yield self._vagrant_stdout

    def _clear_vagrant_log(self):
        with open(self._vagrant_log_fpath, 'w') as f:
            pass

    def _wrap_vagrant_call(self, fn, *args, **kw):
        try:
            return fn(*args, **kw)
        except subprocess.CalledProcessError as x:
            if x.output is None:  # vagrant uses check_call, it returns no output
                with open(self._vagrant_log_fpath) as f:
                    print '=== Vagrant log:  ===============>8 ======================='
                    print f.read()
                    print '================================= 8<======================='
            raise


    def create_vagrant_dir(self):
        if not os.path.exists(self._vagrant_dir):
            os.makedirs(self._vagrant_dir)

    def write_vagrantfile(self, boxes_config):
        template_fpath = os.path.join(TEST_DIR, 'Vagrantfile.jinja2')
        with open(template_fpath) as f:
            template = jinja2.Template(f.read())
        vagrantfile = template.render(
            template_fpath=template_fpath,
            boxes=boxes_config)
        with open(os.path.join(self._vagrant_dir, 'Vagrantfile'), 'w') as f:
            f.write(vagrantfile)

    def _write_ssh_config(self, box_name):
        with open(self._ssh_config_path, 'a') as f:
            f.write(self._vagrant.ssh_config(box_name))

    def make_sudo_command_args(self, box_name, user, args):
        return ['ssh', '-F', self._ssh_config_path, '%s@%s' % (user, box_name)] + list(args)

    def run_ssh_command(self, box_name, user, args):
        args = self.make_sudo_command_args(box_name, user, args)
        print ' '.join(args)
        return subprocess.check_output(args, stderr=subprocess.STDOUT)

    def start_not_started_vagrant_boxes(self):
        missing_boxes = [status.name for status in self._vagrant.status() if status.state != 'running']
        for box_name in missing_boxes:
            print 'Starting/creating box: %r...' % box_name
            self._wrap_vagrant_call(self._vagrant.up, vm_name=box_name)
            self._write_ssh_config(box_name)
            self.run_ssh_command(box_name, 'vagrant', ['sudo', 'cp', '-r', '/home/vagrant/.ssh', '/root/'])

    def _load_box_ip_address(self, box_name):
        output = self.run_ssh_command(box_name, 'root', ['ifconfig', 'eth1'])  # eth0 is nat required by vagrant
        mo = re.search(r'inet addr:([0-9\.]+)', output)
        assert mo, 'Unexpected ifconfig eth0 output:\n' + output
        return mo.group(1)

    def _load_box_timezone(self, box_name):
        tzname = self.run_ssh_command(box_name, 'root', ['date', '+%Z'])
        return pytz.timezone(tzname.rstrip())

    def get_box(self, box_config):
        ip_address = self._load_box_ip_address(box_config.name)
        timezone = self._load_box_timezone(box_config.name)
        return VagrantBox(self, box_config, ip_address, timezone)


class VagrantBox(object):

    def __init__(self, vagrant, box_config, ip_address, timezone):
        self._vagrant = vagrant
        self._name = box_config.name
        self.ip_address = ip_address  # str
        self.timezone = timezone  # timezone
        self.servers = []

    def __repr__(self):
        return 'Box%r@%s/%s' % (self._name, self.ip_address, self.timezone)

    def _make_sudo_command_args(self, args, user=None):
        return self._vagrant.make_sudo_command_args(self._name, user or 'root', args)

    def run_ssh_command(self, args, user=None):
        return self._vagrant.run_ssh_command(self._name, user or 'root', args)

    def run_ssh_command_with_input(self, args, input, user=None):
        args = self._make_sudo_command_args(args, user)
        print ' '.join(args), '(with %s bytes input)' % len(input)
        pipe = subprocess.Popen(
            args,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            stdin=subprocess.PIPE)
        out, err = pipe.communicate(input)
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
