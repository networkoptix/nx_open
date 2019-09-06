from vms_benchmark import exceptions
from os import pipe, fdopen, close, environ, dup2, execvpe, kill, waitpid
import time
import sys
import platform
import subprocess
from io import StringIO

if platform.system() == 'Linux':
    from os import O_NONBLOCK, fork
    from fcntl import fcntl, F_GETFL, F_SETFL

    class DeviceConnection:
        class DeviceConnectionResult:
            def __init__(self, return_code, message=None):
                self.message = message
                self.return_code = return_code

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
                    "-T",
                    f"-p{port}",
                    f"{login}@{host}" if login else host,
                    "sh", "-i"
                ]
            else:
                self.ssh_command = 'ssh'
                self.ssh_args = ['ssh', "-t", f"-p{port}", f"{login}@{host}" if login else host, "sh", "-i"]
            self.fds = {
                'stdout': pipe(),
                'stderr': pipe(),
                'stdin': pipe(),
            }

            for fd in ('stdout', 'stderr'):
                fcntl(self.fds[fd][0], F_SETFL, fcntl(self.fds[fd][0], F_GETFL) | O_NONBLOCK)
            for fd in ('stdout', 'stderr'):
                setattr(self, fd, fdopen(self.fds[fd][0], 'r'))
            self.stdin = fdopen(self.fds['stdin'][1], 'w', buffering=1)

            pid = fork()

            if pid == 0:
                dup2(self.fds['stdin'][0], 0)
                dup2(self.fds['stdout'][1], 1)
                dup2(self.fds['stderr'][1], 2)
                execvpe(self.ssh_command, self.ssh_args, environ)

            self.pid = pid

            time.sleep(0.1)

            def is_process_running(pid):
                try:
                    kill(pid, 0)
                    return True
                except OSError:
                    return False

            self.stdin.write("PS1=$(printf '\\03')\n")
            while True:
                time.sleep(0.001)
                ch = self.stderr.read(1)
                if str(ch) == str(chr(3)):
                    break
                else:
                    pass
                if not is_process_running(pid):
                    print("Worker process exited unexpectedly", file=sys.stderr)
                    sys.exit(1)

            self.host_key = None
            self.ip = None
            self.local_ip = None
            self.is_root = False

        def obtain_connection_info(self):
            # Obtain device ip address
            ssh_connection_info = self.eval('echo $SSH_CONNECTION').strip().split()
            self.ip = ssh_connection_info[2]
            self.local_ip = ssh_connection_info[0]
            self.is_root = self.eval('id -u') == '0'

        def _wait_finish_marker(self, stdout_cb=None, stderr_cb=None, timeout=15.):
            started_at = time.time()
            right_sequence = [chr(3)]
            while time.time() - started_at < timeout:
                ch = self.stderr.read(1)
                if ch == '':
                    time.sleep(0.001)
                    continue
                if ch == right_sequence[0]:
                    right_sequence.pop(0)
                    if len(right_sequence) == 0:
                        return True
                    else:
                        continue
                else:
                    if callable(stderr_cb) and not stderr_cb(ch):
                        return False
                if callable(stdout_cb):
                    [stdout_cb(line) for line in self.stdout.readlines()]
            return False

        def _ping(self, timeout=1):
            self.stdout.readlines()
            self.stderr.readlines()
            self.stdin.write("\n")
            return self._wait_finish_marker(stderr_cb=lambda _d: False, timeout=timeout)

        def sh(self, command, timeout=3, su=False, exc=False, stdout=sys.stdout, stderr=sys.stderr):
            command_wrapped = command if self.is_root or not su else f'sudo -n {command}'

            if not self._ping():
                message = f'Remote executor is not responding before command `{command_wrapped}`.'
                if exc:
                    raise exceptions.DeviceCommandError(message=message)
                else:
                    return self.DeviceConnectionResult(None, message)

            self.stdin.write(command_wrapped)
            self.stdin.write("\n")

            def out_print(d):
                write_method = getattr(stdout, 'write', None)
                if callable(write_method):
                    stdout.write(d)
                return True

            def err_print(d):
                write_method = getattr(stderr, 'write', None)
                if callable(write_method):
                    stderr.write(d)
                return True

            if not self._wait_finish_marker(stdout_cb=out_print, stderr_cb=err_print, timeout=timeout):
                message = f'Remote executor is not responding after command `{command_wrapped}`.'
                if exc:
                    raise exceptions.DeviceCommandError(message=message)
                else:
                    return self.DeviceConnectionResult(None, message)

            time.sleep(0.01)
            [out_print(line) for line in self.stdout.readlines()]
            self.stdin.write("echo $?\n")
            if not self._wait_finish_marker(stderr_cb=err_print, timeout=0.1):
                message = f"Can't obtain return code of the command {command_wrapped}"
                if exc:
                    raise exceptions.DeviceCommandError(message=message)
                else:
                    return self.DeviceConnectionResult(None, message)

            return_code = int(''.join(self.stdout.readlines()))

            if return_code != 0 and exc:
                raise exceptions.DeviceCommandError(
                    message=f'Command `{command_wrapped}` failed with code {return_code}'
                )

            return self.DeviceConnectionResult(return_code)

        def eval(self, cmd, timeout=3, su=False, stderr=sys.stderr):
            out = StringIO()
            res = self.sh(cmd, su=su, stdout=out, stderr=stderr, timeout=timeout)

            if not res:
                return None

            return out.getvalue().strip()

        def get_file_content(self, path, su=False, stderr=sys.stderr, timeout=15):
            return self.eval(f'cat "{path}"', su=su, stderr=stderr, timeout=timeout)

        def __del__(self):
            try:
                kill(self.pid, 9)
                waitpid(self.pid, 0)
            except ChildProcessError:
                pass
            [close(self.fds[name][i]) for (name, i) in (('stdin', 1), ('stdout', 0), ('stderr', 0))]

elif platform.system() == 'Windows':
    class DeviceConnection:
        class DeviceConnectionResult:
            def __init__(self, return_code, command=None, message=None):
                self.message = message
                self.return_code = return_code

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
                self._ssh_command = ['plink', '-batch', f"-p{port}", f"{login}@{host}" if login else host, 'sh']

            self.host_key = None
            self.ip = None
            self.local_ip = None
            self.is_root = False

        def obtain_connection_info(self):
            # Obtain device ip address
            ssh_connection_info = self.eval('echo $SSH_CONNECTION').strip().split()
            self.ip = ssh_connection_info[2]
            self.local_ip = ssh_connection_info[0]
            self.is_root = self.eval('id -u') == '0'

        def ssh_command(self):
            res = self.ssh_command.copy()
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
            try:
                proc = subprocess.Popen(self.ssh_command, **opts)
                out, err = proc.communicate(f"{command_wrapped}\n".encode('UTF-8'), timeout)
                proc.stdin.close()
            except subprocess.TimeoutExpired:
                message = f'Timeout {timeout} seconds expired'
                if exc:
                    raise exceptions.DeviceCommandError(message=message)
                else:
                    return self.DeviceConnectionResult(None, message)

            if stdout != self._SH_DEFAULT:
                write_method = getattr(stdout, 'write', None)
                if callable(write_method):
                    stdout.write(out.decode('UTF-8'))
            if stderr != self._SH_DEFAULT:
                write_method = getattr(stderr, 'write', None)
                if callable(write_method):
                    stderr.write(err.decode('UTF-8'))

            if exc and proc.returncode != 0:
                raise exceptions.DeviceCommandError(
                    message=f'Command `{command_wrapped}` failed with code {proc.returncode}'
                )

            return self.DeviceConnectionResult(proc.returncode, command=command_wrapped)

        def eval(self, cmd, timeout=3, su=False, stderr=sys.stderr):
            out = StringIO()
            res = self.sh(cmd, su=su, stdout=out, stderr=stderr, timeout=timeout)

            if not res:
                return None

            return out.getvalue().strip()

        def get_file_content(self, path, su=False, stderr=sys.stderr, timeout=15):
            return self.eval(f'cat "{path}"', su=su, stderr=stderr, timeout=timeout)

else:
    raise Exception("ERROR: OS is unsupported.")
