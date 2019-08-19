from os import pipe, fdopen, close, O_NONBLOCK, environ, fork, dup2, execvpe, kill, waitpid
from fcntl import fcntl, F_GETFL, F_SETFL
import time
import sys
from io import StringIO


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
            self.ssh_command = '/usr/bin/sshpass'
            self.ssh_args = [
                '/usr/bin/sshpass',
                "-p", password,
                "ssh",
                "-o", "StrictHostKeyChecking=no",
                "-T",
                f"-p{port}",
                f"{login}@{host}" if login else host,
                "sh", "-i"
            ]
        else:
            self.ssh_command = '/usr/bin/ssh'
            self.ssh_args = ["-t", f"-p{port}", host, "sh", "-i"]
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

        # Obtain device ip address
        ssh_connection_info = self.eval('echo $SSH_CONNECTION').strip().split()
        self.ip = ssh_connection_info[2]
        self.local_ip = ssh_connection_info[0]

    def _wait_finish_marker(self, stdout_cb=None, stderr_cb=None, timeout=15):
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

    def sh(self, command, timeout=3, stdout=sys.stdout, stderr=sys.stderr):
        if not self._ping():
            return self.DeviceConnectionResult(None, "Remove executor is not responding (before execution)")
        self.stdin.write(command)
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
            return self.DeviceConnectionResult(None, "Remote executor is not responding (after execution)")
        time.sleep(0.01)
        [out_print(line) for line in self.stdout.readlines()]
        self.stdin.write("echo $?\n")
        if not self._wait_finish_marker(stderr_cb=err_print, timeout=0.1):
            return self.DeviceConnectionResult(None, "Can't obtain return code of the command")
        return self.DeviceConnectionResult(int(''.join(self.stdout.readlines())))

    def eval(self, cmd, timeout=3, stderr=sys.stderr):
        out = StringIO()
        res = self.sh(cmd, stdout=out, stderr=stderr, timeout=timeout)

        if not res:
            return None

        return out.getvalue().strip()

    def get_file_content(self, path, stderr=sys.stderr, timeout=15):
        return self.eval(f'cat "{path}"', stderr=stderr, timeout=timeout)

    def __del__(self):
        kill(self.pid, 9)
        waitpid(self.pid, 0)
        close(self.fds['stdin'][1])
        close(self.fds['stdout'][0])
        close(self.fds['stderr'][0])
