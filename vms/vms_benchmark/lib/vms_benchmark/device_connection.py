import logging

from vms_benchmark import exceptions
import sys
import platform
import subprocess
from io import StringIO

def log_remote_command(command):
    logging.info(f'Executing remote command:\n    {command}')


def log_remote_command_status(status_code):
    if status_code == 0:
        result_log_message = 'succeeded'
    else:
        result_log_message = f'failed with code {status_code}'
    logging.info(f'Remote command {result_log_message}')


if platform.system() == 'Linux':
    class DeviceConnection:
        class DeviceConnectionResult:
            def __init__(self, return_code, message=None, command=None):
                self.message = message
                self.return_code = return_code
                self.command = command

            def __bool__(self):
                return self.return_code == 0

            def message(self):
                return self.message

        def __init__(self, proto='ssh', host='127.0.0.1', port=80, login=None, password=None):
            self.host = host
            self.port = port
            self.login = login
            self.password = password
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

        def obtain_connection_info(self):
            # Obtain device ip address
            eval_reply = self.eval('echo $SSH_CONNECTION')
            ssh_connection_info = eval_reply.strip().split() if eval_reply else None
            if not eval_reply or len(ssh_connection_info) < 3:
                raise exceptions.DeviceCommandError(
                    'Unable to connect to the box via ssh; check deviceLogin and devicePassword in vms_benchmark.conf.')
            self.ip = ssh_connection_info[2]
            self.local_ip = ssh_connection_info[0]
            self.is_root = self.eval('id -u') == '0'

        def sh(self, command, timeout=3, su=False, exc=False, stdout=sys.stdout, stderr=None):
            command_wrapped = command if self.is_root or not su else f'sudo -n {command}'

            log_remote_command(command_wrapped)

            try:
                run = subprocess.run([*self.ssh_args, command_wrapped], timeout=timeout, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            except subprocess.TimeoutExpired:
                message = (f'Unable to execute remote command via ssh: timeout of {timeout} seconds expired; ' +
                          'check deviceHost in vms_benchmark.conf.')
                if exc:
                    raise exceptions.DeviceCommandError(message=message)
                else:
                    return self.DeviceConnectionResult(None, message, command=command_wrapped)

            log_remote_command_status(run.returncode)

            if run.returncode == 255:
                if exc:
                    raise exceptions.DeviceCommandError(message=run.stderr.rstrip())
                else:
                    return self.DeviceConnectionResult(None, run.stderr.rstrip(), command=command_wrapped)

            if run.returncode != 0 and exc:
                raise exceptions.DeviceCommandError(
                    message=f'Command `{command_wrapped}` failed with code {run.returncode}, stderr:\n    {run.stderr}'
                )

            if stdout:
                stdout.write(run.stdout.decode())
                stdout.flush()
            if stderr:
                stderr.write(run.stderr.decode())
                stderr.flush()

            return self.DeviceConnectionResult(run.returncode, command=command_wrapped)

        def eval(self, cmd, timeout=3, su=False, stderr=None):
            out = StringIO()
            res = self.sh(cmd, su=su, stdout=out, stderr=stderr, timeout=timeout)

            if res.return_code is None:
                raise exceptions.DeviceCommandError(res.message)

            if not res:
                return None

            return out.getvalue().strip()

        def get_file_content(self, path, su=False, stderr=None, timeout=15):
            return self.eval(f'cat "{path}"', su=su, stderr=stderr, timeout=timeout)

elif platform.system() == 'Windows' or platform.system().startswith('CYGWIN'):
    class DeviceConnection:
        class DeviceConnectionResult:
            def __init__(self, return_code, message=None, command=None):
                self.message = message
                self.return_code = return_code
                self.command = command

            def __bool__(self):
                return self.return_code == 0

            def message(self):
                return self.message

        def __init__(self, proto='ssh', host='127.0.0.1', port=80, login=None, password=None):
            self.host = host
            self.port = port
            self.login = login
            self.password = password
            if password:
                self._ssh_command = [
                    'plink',
                    '-batch',
                    "-pw", password,
                    '-P', str(port),
                    f"{login}@{host}" if login else host,
                    'sh'
                ]
                #self.ssh_command = [
                #    'sshpass',
                #    "-p", password,
                #    "ssh",
                #    "-o", "StrictHostKeyChecking=no",
                #    f"-p{port}",
                #    f"{login}@{host}" if login else host,
                #    'sh'
                #]
            else:
                self._ssh_command = ['plink', '-batch', '-P', str(port), f"{login}@{host}" if login else host, 'sh']

            self.host_key = None
            self.ip = None
            self.local_ip = None
            self.is_root = False

        def obtain_connection_info(self):
            # Obtain device ip address
            eval_reply = self.eval('echo $SSH_CONNECTION')
            ssh_connection_info = eval_reply.strip().split() if eval_reply else None
            if not eval_reply or len(ssh_connection_info) < 3:
                raise exceptions.DeviceCommandError(
                    'Unable to connect to the box via ssh; check deviceLogin and devicePassword in vms_benchmark.conf.')
            self.ip = ssh_connection_info[2]
            self.local_ip = ssh_connection_info[0]
            self.is_root = self.eval('id -u') == '0'

        def ssh_command(self):
            res = self._ssh_command.copy()
            if self.host_key:
                res.insert(2, '-hostkey')
                res.insert(3, self.host_key)
            return res

        _SH_DEFAULT = object()

        def sh(self, command, timeout=3, su=False, exc=False, stdout=sys.stdout, stderr=sys.stderr):
            opts = {
                'stdin': subprocess.PIPE
            }
            if stdout != self._SH_DEFAULT:
                opts['stdout'] = subprocess.PIPE
            if stderr != self._SH_DEFAULT:
                opts['stderr'] = subprocess.PIPE
            command_wrapped = command if self.is_root or not su else f"sudo -n {command}"
            log_remote_command(command_wrapped)
            try:
                proc = subprocess.Popen(self.ssh_command(), **opts)
                out, err = proc.communicate(f"{command_wrapped}\n".encode('UTF-8'), timeout)
                proc.stdin.close()
            except subprocess.TimeoutExpired:
                message = f'Timeout {timeout} seconds expired'
                if exc:
                    raise exceptions.DeviceCommandError(message=message)
                else:
                    return self.DeviceConnectionResult(None, message, command=command_wrapped)

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
                raise exceptions.DeviceCommandError(
                    message=f'Command `{command_wrapped}` failed with code {proc.returncode}'
                )

            return self.DeviceConnectionResult(proc.returncode, command=command_wrapped)

        def eval(self, cmd, timeout=3, su=False, stderr=None):
            out = StringIO()
            res = self.sh(cmd, su=su, stdout=out, stderr=stderr, timeout=timeout)

            if not res:
                return None

            return out.getvalue().strip()

        def get_file_content(self, path, su=False, stderr=sys.stderr, timeout=15):
            return self.eval(f'cat "{path}"', su=su, stderr=stderr, timeout=timeout)

else:
    raise Exception(f"ERROR: OS {platform.system()} is unsupported.")
