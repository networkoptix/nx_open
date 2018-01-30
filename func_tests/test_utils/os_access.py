"""Unified layer of access to OS, locally or via SSH"""

import abc
import datetime
import errno
import glob
import logging
import shutil
import subprocess
import threading

import pytz
import tzlocal
from pathlib2 import Path, PurePosixPath

from .utils import RunningTime, quote

log = logging.getLogger(__name__)


PROCESS_TIMEOUT = datetime.timedelta(hours=1)


class ProcessError(subprocess.CalledProcessError):

    def __str__(self):
        return 'Command "%s" returned non-zero exit status %d:\n%s' % (self.cmd, self.returncode, self.output)

    def __repr__(self):
        return 'ProcessError(%s)' % self


class ProcessTimeoutError(subprocess.CalledProcessError):

    def __init__(self, cmd, timeout):
        subprocess.CalledProcessError.__init__(self, returncode=None, cmd=cmd)
        self.timeout = timeout

    def __str__(self):
        return 'Command "%s" was timed out (timeout is %s)' % (self.cmd, self.timeout)

    def __repr__(self):
        return 'ProcessTimeoutError(%s)' % self


def log_output(name, output):
    if not output:
        return  # do not log ''; skip None also
    if '\0' in output:
        log.debug('\t--> %s: %s bytes binary', name, len(output))
    elif len(output) > 200:
        log.debug('\t--> %s: %r...', name, output[:200])
    else:
        log.debug('\t--> %s: %r',  name, output.rstrip('\r\n'))


class SshAccessConfig(object):

    @classmethod
    def from_dict(cls, d):
        assert isinstance(d, dict), 'ssh location should be dict: %r'  % d
        assert 'host' in d, '"host" is required parameter for host location'
        return cls(
            name=d.get('name') or d['hostname'],
            hostname=d['hostname'],
            user=d.get('user'),
            key_file_path=d.get('key_file'),
            )

    def __init__(self, name, hostname, user=None, key_file_path=None):
        self.name = name
        self.hostname = hostname
        self.user = user
        self.key_file_path = key_file_path

    def __repr__(self):
        return '<os_access=%r user=%r key_file=%r>' % (self.hostname, self.user, self.key_file_path)


def host_from_config(config):
    if config:
        return SshAccess(config.os_access, name=config.name, user=config.user, key_path=config.key_file_path)
    else:
        return LocalAccess()


class OsAccess(object):

    __metaclass__ = abc.ABCMeta

    @abc.abstractmethod
    def run_command(self, args, input=None, cwd=None, check_retcode=True, log_output=True, timeout=None):
        pass

    @abc.abstractmethod
    def file_exists(self, path):
        pass

    @abc.abstractmethod
    def dir_exists(self, path):
        pass

    @abc.abstractmethod
    def put_file(self, from_local_path, to_remote_path):
        pass

    @abc.abstractmethod
    def get_file(self, from_remote_path, to_local_path):
        pass

    @abc.abstractmethod
    def read_file(self, from_remote_path, ofs=None):
        pass

    @abc.abstractmethod
    def write_file(self, to_remote_path, contents):
        pass

    @abc.abstractmethod
    def get_file_size(self, path):
        pass

    # create parent dirs as well
    @abc.abstractmethod
    def mk_dir(self, path):
        pass

    @abc.abstractmethod
    def rm_tree(self, path, ignore_errors=False):
        pass

    @abc.abstractmethod
    def expand_path(self, path):
        pass

    # expand path with '*' mask into list of paths
    # if for_shell=True, returns path_mask as is for remote ssh host -
    #   it is assumed to be used in shell command then, and shell will expand it by itself
    @abc.abstractmethod
    def expand_glob(self, path_mask, for_shell=False):
        pass

    @abc.abstractmethod
    def get_timezone(self):
        pass

    def set_time(self, new_time):
        started_at = datetime.datetime.now(pytz.utc)
        self.run_command(['date', '--set', new_time.isoformat()])
        return RunningTime(new_time, datetime.datetime.now(pytz.utc) - started_at)

    def make_proxy_command(self):
        return []


class LocalAccess(OsAccess):
    def __init__(self):
        self.name = 'localhost'
        self.hostname = 'localhost'

    def __repr__(self):
        return '<LocalAccess>'

    def run_command(self, args, input=None, cwd=None, check_retcode=True, log_output=True, timeout=None):
        timeout = timeout or PROCESS_TIMEOUT
        args = [str(arg) for arg in args]
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
            stdin=stdin,
            close_fds=True,
            )
        stdout_buffer = []
        stderr_buffer = []
        stdout_thread = threading.Thread(target=self._read_thread, args=(log.debug, pipe.stdout, stdout_buffer, log_output))
        stderr_thread = threading.Thread(target=self._read_thread, args=(log.error, pipe.stderr, stderr_buffer, True))
        stdout_thread.daemon = True
        stderr_thread.daemon = True
        stdout_thread.start()
        stderr_thread.start()
        try:
            try:
                if input:
                    try:
                        pipe.stdin.write(input)
                    except IOError as e:
                        if e.errno == errno.EPIPE:
                            # communicate() should ignore broken pipe error
                            pass
                        elif e.errno == errno.EINVAL and pipe.poll() is not None:
                            # stdin.write() fails with EINVAL if the process already exited before the write
                            pass
                        else:
                            raise
                    pipe.stdin.close()
            finally:
                stdout_thread.join(timeout=timeout.total_seconds())
                if not stdout_thread.isAlive():
                    stderr_thread.join(timeout=timeout.total_seconds())
                if stdout_thread.isAlive() or stderr_thread.isAlive():
                    pipe.kill()
                    raise ProcessTimeoutError(subprocess.list2cmdline(args), timeout)
        finally:
            stdout_thread.join()
            stderr_thread.join()
            pipe.stdout.close()
            pipe.stderr.close()
        retcode = pipe.wait()
        if check_retcode and retcode:
            raise ProcessError(retcode, args[0], output=''.join(stderr_buffer))
        return ''.join(stdout_buffer)

    def _read_thread(self, logger, f, buffer, log_lines):
        is_binary = False
        while True:
            line = f.readline()
            if not line:
                return
            if '\0' in line:
                is_binary = True
            if log_lines and not is_binary:
                logger('\t> %s' % ''.join(map(self._mask_ws, line.rstrip('\r\n'))))
            buffer.append(line)

    def _mask_ws(self, ch):
        if ch == '\t' or ch >= ' ':
            return ch
        else:
            return '.'

    def file_exists(self, path):
        return self.expand_path(path).exists()

    def dir_exists(self, path):
        return self.expand_path(path).is_dir()

    def put_file(self, from_local_path, to_remote_path):
        self._copy(self.expand_path(str(from_local_path)), str(to_remote_path))

    def get_file(self, from_remote_path, to_local_path):
        self._copy(from_remote_path, self.expand_path(to_local_path))

    def _copy(self, from_path, to_path):
        from_path = self.expand_path(from_path)
        to_path = self.expand_path(to_path)
        log.debug('copying %s -> %s', from_path, to_path)
        shutil.copy2(from_path, to_path)

    def read_file(self, from_remote_path, ofs=None):
        with from_remote_path.open('rb') as f:
            if ofs:
                f.seek(ofs)
            return f.read()

    def write_file(self, to_remote_path, contents):
        log.debug('writing %d bytes to %s', len(contents), to_remote_path)
        self.mk_dir(to_remote_path.parent)
        to_remote_path.write_bytes(contents)

    def get_file_size(self, path):
        return self.expand_path(path).st_size

    def mk_dir(self, path):
        self.expand_path(path).mkdir(parents=True, exist_ok=True)

    def rm_tree(self, path, ignore_errors=False):
        shutil.rmtree(str(self.expand_path(path)), ignore_errors=ignore_errors)

    def expand_path(self, path):
        return Path(path).expanduser().resolve()

    def expand_glob(self, path_mask, for_shell=False):
        return [Path(path) for path in glob.glob(str(path_mask))]

    def get_timezone(self):
        return tzlocal.get_localzone()


class SshAccess(OsAccess):

    def __init__(self, hostname, user=None, name=None, key_path=None, config_path=None, proxy_os_access=None):
        self.hostname = hostname
        self.name = name or self.hostname
        if user is None:
            self._user_and_host = self.hostname
        else:
            self._user_and_host = user + '@' + self.hostname
        self._key_file_path = key_path
        self._config_path = config_path
        assert proxy_os_access is None or isinstance(proxy_os_access, OsAccess), repr(proxy_os_access)
        self._proxy_os_access = proxy_os_access or LocalAccess()
        self._local_os_access = LocalAccess()

    def __repr__(self):
        return '<{} {} config_path={}>'.format(self.__class__.__name__, self._user_and_host, self._config_path)

    def run_command(self, args, input=None, cwd=None, check_retcode=True, log_output=True, timeout=None):
        ssh_cmd = self._make_ssh_cmd() + [self._user_and_host]
        if cwd:
            args = [subprocess.list2cmdline(['cd', str(cwd), '&&'] + [str(arg) for arg in args])]
        return self._local_os_access.run_command(ssh_cmd + args, input, check_retcode=check_retcode, log_output=log_output, timeout=timeout)

    def file_exists(self, path):
        output = self.run_command(['[', '-f', path, ']', '&&', 'echo', 'yes', '||', 'echo', 'no']).strip()
        assert output in ['yes', 'no'], repr(output)
        return output == 'yes'

    def dir_exists(self, path):
        output = self.run_command(['[', '-d', path, ']', '&&', 'echo', 'yes', '||', 'echo', 'no']).strip()
        assert output in ['yes', 'no'], repr(output)
        return output == 'yes'

    def put_file(self, from_local_path, to_remote_path):
        self._local_os_access.run_command([
            'rsync',  # If file size and modification time is same, rsync doesn't copy file.
            '--archive',  # Preserve directory structure, times, access rights.
            '--copy-links',  # From help: "transform symlink into referent file/dir".
            '--rsh=' + ' '.join(self._make_ssh_cmd(must_quote=True)),
            from_local_path,  # Support pathlib.
            self._user_and_host + ':' + str(to_remote_path),  # Support pathlib.
            ])

    def get_file(self, from_remote_path, to_local_path):
        cmd = ['rsync', '-aL', '-e', subprocess.list2cmdline(self._make_ssh_cmd(must_quote=True)),
               '{user_and_host}:"{path}"'.format(user_and_host=self._user_and_host, path=from_remote_path),
               to_local_path]
        self._local_os_access.run_command(cmd)

    def read_file(self, from_remote_path, ofs=None):
        if ofs:
            assert type(ofs) is int, repr(ofs)
            return self.run_command(['tail', '--bytes=+%d' % ofs, from_remote_path], log_output=False)
        else:
            return self.run_command(['cat', from_remote_path], log_output=False)
        
    def write_file(self, to_remote_path, contents):
        to_remote_path = PurePosixPath(to_remote_path)
        self.run_command(['mkdir', '-p', to_remote_path.parent, '&&', 'cat', '>', to_remote_path], contents)

    def get_file_size(self, path):
        return self.run_command(['stat', '--format=%s', path])

    def mk_dir(self, path):
        self.run_command(['mkdir', '-p', str(path)])

    def rm_tree(self, path, ignore_errors=False):
        self.run_command(['rm', '-r', path], check_retcode=not ignore_errors)

    def expand_path(self, path):
        assert '*' not in path, repr(path)  # this function is only for expanding ~ and $VARs. for globs use expand_glob
        return self.run_command(['echo', path]).strip()

    def expand_glob(self, path_mask, for_shell=False):
        if not path_mask.is_absolute():
            raise ValueError("Path mask must be absolute, not %r", path_mask)
        if for_shell:
            return [str(path_mask)]  # ssh expands '*' masks automatically
        else:
            fixed_root = next(parent for parent in path_mask.parent.parents if '*' not in str(parent))
            output = self.run_command(['find', fixed_root, '-wholename',  "'" + str(path_mask) + "'"]).strip()
            return [PurePosixPath(path) for path in output.splitlines()]

    def get_timezone(self):
        tzname = self.read_file('/etc/timezone').strip()
        return pytz.timezone(tzname)
        
    def make_proxy_command(self):
        return self._make_ssh_cmd() + [self._user_and_host, 'nc', '-q0', '%h', '%p']

    def _make_ssh_cmd(self, must_quote=False):
        return ['ssh'] + self._make_identity_args() + self._make_config_args() + self._make_proxy_args(must_quote)

    def _make_identity_args(self):
        if self._key_file_path:
            return ['-i', str(self._key_file_path)]
        else:
            return []

    def _make_config_args(self):
        if self._config_path:
            return ['-F', str(self._config_path)]
        else:
            return []

    def _make_proxy_args(self, must_quote):
        proxy_command = self._proxy_os_access.make_proxy_command()
        if proxy_command:
            cmd = 'ProxyCommand=%s' % ' '.join(proxy_command)
            if must_quote:
                cmd = quote(cmd, "'")
            return ['-o', cmd]
        else:
            return []
