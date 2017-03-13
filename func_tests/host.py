'''Abstraction over 'host' - local or remote one via ssh.

Allows running commands or working with files on local or remote hosts transparently.
'''

import abc
import os
import os.path
import logging
import threading
import subprocess
import errno
import shutil


log = logging.getLogger(__name__)


class ProcessError(subprocess.CalledProcessError):

    def __str__(self):
        return 'Command "%s" returned non-zero exit status %d:\n%s' % (self.cmd, self.returncode, self.output)

    def __repr__(self):
        return 'ProcessError(%s)' % self


def log_output(name, output):
    if not output:
        return  # do not log ''; skip None also
    if '\0' in output:
        log.debug('\t--> %s: %s bytes binary', name, len(output))
    elif len(output) > 200:
        log.debug('\t--> %s: %r...', name, output[:200])
    else:
        log.debug('\t--> %s: %r',  name, output.rstrip('\r\n'))


class SshHostConfig(object):

    def __init__(self, host, user, key_file_path):
        self.host = host
        self.user = user
        self.key_file_path = key_file_path


def host_from_config(config):
    if config:
        return RemoteSshHost(config.host, config.user, config.key_file_path)
    else:
        return LocalHost()


class Host(object):

    __metaclass__ = abc.ABCMeta

    @abc.abstractmethod
    def run_command(self, args, input=None, cwd=None):
        pass

    @abc.abstractmethod
    def put_file(self, from_local_path, to_remote_path):
        pass

    @abc.abstractmethod
    def get_file(self, from_remote_path, to_local_path):
        pass

    @abc.abstractmethod
    def read_file(self, from_remote_path):
        pass

    @abc.abstractmethod
    def write_file(self, to_remote_path, contents):
        pass

    def make_proxy_command(self):
        return []

    
class LocalHost(Host):

    def run_command(self, args, input=None, cwd=None):
        if input:
            log.debug('executing: %s (with %d bytes input)', subprocess.list2cmdline(args), len(input))
            stdin = subprocess.PIPE
        else:
            log.debug('executing: %s', subprocess.list2cmdline(args))
            stdin = None
        pipe = subprocess.Popen(
            args, cwd=cwd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            stdin=stdin)
        stdout_buffer = []
        stderr_buffer = []
        stdout_thread = threading.Thread(target=self._read_thread, args=(log.debug, pipe.stdout, stdout_buffer))
        stderr_thread = threading.Thread(target=self._read_thread, args=(log.error, pipe.stderr, stderr_buffer))
        stdout_thread.daemon = True
        stderr_thread.daemon = True
        stdout_thread.start()
        stderr_thread.start()
        try:
            if input:
                try:
                    pipe.stdin.write(input)
                except IOError as e:
                    if e.errno == errno.EPIPE:
                        # communicate() should ignore broken pipe error
                        pass
                    elif (e.errno == errno.EINVAL and pipe.poll() is not None):
                        # stdin.write() fails with EINVAL if the process already exited before the write
                        pass
                    else:
                        raise
                pipe.stdin.close()
        finally:
            stdout_thread.join()
            stderr_thread.join()
            pipe.stdout.close()
            pipe.stderr.close()
        retcode = pipe.wait()
        if retcode or stderr_buffer:
            raise ProcessError(retcode, args[0], output=''.join(stderr_buffer))
        return ''.join(stdout_buffer)

    def _read_thread(self, logger, f, buffer):
        is_binary = False
        while True:
            line = f.readline()
            if not line:
                return
            if '\0' in line:
                is_binary = True
            if not is_binary and logger:
                logger('\t> %s' % ''.join(map(self._mask_ws, line.rstrip('\r\n'))))
            buffer.append(line)

    def _mask_ws(self, ch):
        if ch == '\t' or ch >= ' ':
            return ch
        else:
            return '.'

    def put_file(self, from_local_path, to_remote_path):
        self._copy(from_local_path, to_remote_path)

    def get_file(self, from_remote_path, to_local_path):
        self._copy(from_remote_path, to_local_path)

    def _copy(self, from_path, to_path):
        log.debug('copying %s -> %s', from_path, to_path)
        shutil.copy2(from_path, to_path)

    def read_file(self, from_remote_path):
        with open(from_remote_path) as f:
            return f.read()
        
    def write_file(self, to_remote_path, contents):
        log.debug('writing %d bytes to %s', len(contents), to_remote_path)
        dir = os.path.dirname(to_remote_path)
        if not os.path.isdir(dir):
            os.makedirs(dir)
        with open(to_remote_path, 'w') as f:
            f.write(contents)


class RemoteSshHost(Host):

    def __init__(self, host, user, key_file_path=None, ssh_config_path=None, proxy_host=None):
        assert proxy_host is None or isinstance(proxy_host, Host), repr(proxy_host)
        self._proxy_host = proxy_host or LocalHost()
        self._host = host
        self._user = user
        self._key_file_path = key_file_path
        self._ssh_config_path = ssh_config_path
        self._local_host = LocalHost()

    def run_command(self, args, input=None, cwd=None):
        ssh_cmd = self._make_ssh_cmd() + ['{user}@{host}'.format(user=self._user, host=self._host)]
        if cwd:
            args = [subprocess.list2cmdline(['cd', cwd, '&&'] + args)]
        return self._local_host.run_command(ssh_cmd + args, input)

    def put_file(self, from_local_path, to_remote_path):
        #assert not self._proxy_host, repr(self._proxy_host)  # Can not proxy this... Or can we?
        cmd = ['rsync', '-aL', '-e', ' '.join(self._make_ssh_cmd()),
               from_local_path,
               '{user}@{host}:{path}'.format(user=self._user, host=self._host, path=to_remote_path)]
        self._local_host.run_command(cmd)

    def get_file(self, from_remote_path, to_local_path):
        cmd = ['rsync', '-aL', '-e', subprocess.list2cmdline(self._make_ssh_cmd()),
               '{user}@{host}:{path}'.format(user=self._user, host=self._host, path=from_remote_path),
               to_local_path]
        self._local_host.run_command(cmd)

    def read_file(self, from_remote_path):
        return self.run_command(['cat', from_remote_path])
        
    def write_file(self, to_remote_path, contents):
        remote_dir = os.path.dirname(to_remote_path)
        self.run_command(['mkdir', '-p', remote_dir, '&&', 'cat', '>', to_remote_path], contents)

    def make_proxy_command(self):
        return (self._make_ssh_cmd()
                + ['{user}@{host}'.format(user=self._user, host=self._host),
                   'nc', '-q0', '%h', '%p'])

    def _make_ssh_cmd(self):
        return ['ssh'] + self._make_identity_args() + self._make_config_args() + self._make_proxy_args()

    def _make_identity_args(self):
        if self._key_file_path:
            return ['-i', self._key_file_path]
        else:
            return []

    def _make_config_args(self):
        if self._ssh_config_path:
            return ['-F', self._ssh_config_path]
        else:
            return []

    def _make_proxy_args(self):
        proxy_command = self._proxy_host.make_proxy_command()
        if proxy_command:
            return ['-o', 'ProxyCommand=%s' % ' '.join(proxy_command)]
        else:
            return []
