import logging
import platform
import subprocess
import sys
from contextlib import contextmanager
from io import StringIO

from vms_benchmark import exceptions

ini_ssh_command_timeout_s: int
ini_ssh_get_file_content_timeout_s: int


def log_remote_command(command):
    logging.info(f'Executing remote command:\n    {command}')


def log_remote_command_status(status_code):
    if status_code == 0:
        result_log_message = 'succeeded.'
    else:
        result_log_message = f'failed with exit status {status_code}.'
    logging.info(f'Remote command {result_log_message}')


if platform.system() == 'Linux':
    class BoxConnection:
        class BoxConnectionResult:
            def __init__(self, return_code, message=None, command=None):
                self.message = message
                self.return_code = return_code
                self.command = command

            def __bool__(self):
                return self.return_code == 0

            def message(self):
                return self.message

        def __init__(self, host, port, login, password, conf_file):
            self.host = host
            self.port = port
            self.login = login
            self.password = password
            self.conf_file = conf_file
            if password:
                self.ssh_command = 'sshpass'
                self.ssh_args = [
                    'sshpass',
                    "-p", password,
                    "ssh",
                    "-o", "StrictHostKeyChecking=no",
                    "-o", "PubkeyAuthentication=no",
                    "-o", "PasswordAuthentication=yes",
                    "-T",
                    f"-p{port}",
                    f"{login}@{host}" if login else host,
                ]
            else:
                self.ssh_command = 'ssh'
                self.ssh_args = [
                    'ssh',
                    f"-p{port}",
                    f"{login}@{host}" if login else host,
                    "-o", "StrictHostKeyChecking=no",
                    "-o", "BatchMode=yes",
                    "-o", "PubkeyAuthentication=yes",
                    "-o", "PasswordAuthentication=no",
                    ]

            self.host_key = None
            self.ip = None
            self.local_ip = None
            self.is_root = False
            self.eth_name = None
            self.eth_speed = None

        def obtain_connection_info(self):
            ssh_connection_var_value = self.eval('echo $SSH_CONNECTION')
            ssh_connection_info = ssh_connection_var_value.strip().split() if ssh_connection_var_value else None
            if not ssh_connection_var_value or len(ssh_connection_info) < 3:
                raise exceptions.BoxCommandError(
                    f'Unable to connect to the box via ssh; check box credentials in {repr(self.conf_file)}.')

            self.ip = ssh_connection_info[2]
            self.local_ip = ssh_connection_info[0]
            self.is_root = self.eval('id -u') == '0'

            line_form_with_eth_name = self.eval(f'ip a | grep {self.ip}')
            eth_name = line_form_with_eth_name.split()[-1] if line_form_with_eth_name else None
            if not eth_name:
                raise exceptions.BoxCommandError(
                    f'Unable to detect box network adapter which serves ip {self.ip}.')

            eth_dir = f'/sys/class/net/{eth_name}'
            eth_name_check_result = self.sh(f'test -d "{eth_dir}"')

            if not eth_name_check_result or eth_name_check_result.return_code != 0:
                raise exceptions.BoxCommandError(
                    f'Unable to find box network adapter info dir {repr(eth_dir)}.')

            self.eth_name = eth_name
            eth_speed = self.eval(f'cat /sys/class/net/{self.eth_name}/speed')
            self.eth_speed = eth_speed.strip() if eth_speed else None

        @contextmanager
        def sh2(self, command, su=False):
            command_wrapped = command if self.is_root or not su else f'sudo -n {command}'

            log_remote_command(command_wrapped)

            proc = subprocess.Popen(
                [*self.ssh_args, command_wrapped],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )

            try:
                yield proc
            finally:
                proc.terminate()

        def sh(self, command, timeout_s=None,
               su=False, exc=False, stdout=sys.stdout, stderr=None, stdin=None):
            command_wrapped = command if self.is_root or not su else f'sudo -n {command}'

            log_remote_command(command_wrapped)

            opts = {}

            if stdin:
                opts['input'] = stdin.encode('UTF-8')

            try:
                actual_timeout_s = timeout_s or ini_ssh_command_timeout_s
                run = subprocess.run(
                    [*self.ssh_args, command_wrapped],
                    timeout=actual_timeout_s,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    **opts
                )
            except subprocess.TimeoutExpired:
                message = (f'Unable to execute remote command via ssh: '
                    f'timeout of {actual_timeout_s} seconds expired; ' +
                    f'check boxHostnameOrIp in {repr(self.conf_file)}.')
                if exc:
                    raise exceptions.BoxCommandError(message=message)
                else:
                    return self.BoxConnectionResult(None, message, command=command_wrapped)

            log_remote_command_status(run.returncode)

            if run.returncode == 255:
                if exc:
                    raise exceptions.BoxCommandError(message=run.stderr.decode('UTF-8').rstrip())
                else:
                    return self.BoxConnectionResult(
                        None,
                        run.stderr.decode('UTF-8').rstrip(), command=command_wrapped
                    )

            if run.returncode != 0 and exc:
                raise exceptions.BoxCommandError(
                    f'Command `{command_wrapped}` failed '
                    f'with exit status {run.returncode}, '
                    f'stderr:\n    {run.stderr.decode("UTF-8")}'
                )

            if stdout:
                stdout.write(run.stdout.decode())
                stdout.flush()
            if stderr:
                stderr.write(run.stderr.decode())
                stderr.flush()
            else:
                if run.stderr:
                    logging.debug('Remote command stderr:\n' + run.stderr.decode())

            return self.BoxConnectionResult(run.returncode, command=command_wrapped)

        def eval(self, cmd, timeout_s=None, su=False, stderr=None, stdin=None):
            out = StringIO()
            res = self.sh(cmd, su=su, stdout=out, stderr=stderr, stdin=stdin, timeout_s=timeout_s)

            if res.return_code is None:
                raise exceptions.BoxCommandError(res.message)

            if not res:
                return None

            return out.getvalue().strip()

        def get_file_content(self, path, su=False, stderr=None, stdin=None, timeout_s=None):
            return self.eval(
                f'cat "{path}"', su=su, stderr=stderr, stdin=stdin,
                timeout_s=timeout_s or ini_ssh_get_file_content_timeout_s)

elif platform.system() == 'Windows' or platform.system().startswith('CYGWIN'):
    class BoxConnection:
        class BoxConnectionResult:
            def __init__(self, return_code, message=None, command=None):
                self.message = message
                self.return_code = return_code
                self.command = command

            def __bool__(self):
                return self.return_code == 0

            def message(self):
                return self.message

        def __init__(self, host, port, login, password, conf_file):
            self.host = host
            self.port = port
            self.login = login
            self.password = password
            self.conf_file = conf_file
            if password:
                self._ssh_command = [
                    'plink',
                    '-batch',
                    "-pw", password,
                    '-P', str(port),
                    f"{login}@{host}" if login else host,
                ]
                #self.ssh_command = [
                #    'sshpass',
                #    "-p", password,
                #    "ssh",
                #    "-o", "StrictHostKeyChecking=no",
                #    f"-p{port}",
                #    f"{login}@{host}" if login else host,
                #]
            else:
                self._ssh_command = ['plink', '-batch', '-P', str(port), f"{login}@{host}" if login else host]

            self.host_key = None
            self.ip: str = None
            self.local_ip = None
            self.is_root = False
            self.eth_name = None
            self.eth_speed = None

        def obtain_connection_info(self):
            ssh_connection_var_value = self.eval('echo $SSH_CONNECTION')
            ssh_connection_info = ssh_connection_var_value.strip().split() if ssh_connection_var_value else None
            if not ssh_connection_var_value or len(ssh_connection_info) < 3:
                raise exceptions.BoxCommandError(
                    f'Unable to connect to the box via ssh; check box credentials in {repr(self.conf_file)}.')

            self.ip = ssh_connection_info[2]
            self.local_ip = ssh_connection_info[0]
            self.is_root = self.eval('id -u') == '0'

            line_form_with_eth_name = self.eval(f'ip a | grep {self.ip}')
            eth_name = line_form_with_eth_name.split()[-1] if line_form_with_eth_name else None
            if not eth_name:
                raise exceptions.BoxCommandError(
                    f'Unable to detect box network adapter which serves ip {self.ip}.')

            eth_dir = f'/sys/class/net/{eth_name}'
            eth_name_check_result = self.sh(f'test -d "{eth_dir}"')

            if not eth_name_check_result or eth_name_check_result.return_code != 0:
                raise exceptions.BoxCommandError(
                    f'Unable to find box network adapter info dir {repr(eth_dir)}.')

            self.eth_name = eth_name
            eth_speed = self.eval(f'cat /sys/class/net/{self.eth_name}/speed')
            self.eth_speed = eth_speed.strip() if eth_speed else None

        def ssh_command(self, command):
            res = self._ssh_command.copy()
            if self.host_key:
                res.insert(2, '-hostkey')
                res.insert(3, self.host_key)
            res += [command]
            #logging.info("Executing command: %r", res)
            return res

        _SH_DEFAULT = object()

        def sh(self, command, timeout_s=None, su=False, exc=False,
               stdout=None, stderr=None, stdin=None):
            opts = {
                'stdin': subprocess.PIPE
            }
            if stdout != self._SH_DEFAULT:
                opts['stdout'] = subprocess.PIPE
            if stderr != self._SH_DEFAULT:
                opts['stderr'] = subprocess.PIPE
            command_wrapped = command if self.is_root or not su else f"sudo -n {command}"
            log_remote_command(command_wrapped)
            actual_timeout_s = timeout_s or ini_ssh_command_timeout_s
            try:
                proc = subprocess.Popen(self.ssh_command(command_wrapped), **opts)
                out, err = proc.communicate(stdin.encode('UTF-8') if stdin else b'',
                    actual_timeout_s)
            except subprocess.TimeoutExpired:
                message = f'Timeout {actual_timeout_s} seconds expired'
                if exc:
                    raise exceptions.BoxCommandError(message=message)
                else:
                    return self.BoxConnectionResult(None, message, command=command_wrapped)

            if stdout != self._SH_DEFAULT:
                write_method = getattr(stdout, 'write', None)
                if callable(write_method):
                    stdout.write(out.decode('UTF-8'))
            if stderr != self._SH_DEFAULT:
                write_method = getattr(stderr, 'write', None)
                if callable(write_method):
                    stderr.write(err.decode('UTF-8'))

            log_remote_command_status(proc.returncode)
            
            if exc and proc.returncode != 0:
                raise exceptions.BoxCommandError(
                    f'Command `{command_wrapped}` failed '
                    f'with exit status {proc.returncode}, '
                    f'stderr:\n    {err.decode("UTF-8")}'
                )

            return self.BoxConnectionResult(proc.returncode, command=command_wrapped)

        def eval(self, cmd, timeout_s=None, su=False, stderr=None, stdin=None):
            out = StringIO()
            res = self.sh(cmd, su=su, stdout=out, stderr=stderr, stdin=stdin, timeout_s=timeout_s)

            if not res:
                return None

            return out.getvalue().strip()

        def get_file_content(self, path, su=False, stderr=sys.stderr, stdin=None, timeout_s=None):
            return self.eval(
                f'cat "{path}"', su=su, stderr=stderr, stdin=stdin,
                timeout_s=timeout_s or ini_ssh_get_file_content_timeout_s)

else:
    raise Exception(f"ERROR: OS {platform.system()} is unsupported.")
