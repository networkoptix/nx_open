from os import pipe, fdopen, O_NONBLOCK, O_CLOEXEC, environ, fork, dup2, chdir, execvpe, kill
from fcntl import fcntl, F_GETFL, F_SETFL, F_SETFD
import time
import sys

from pprint import pprint

class ServerConnection:
    class ServerConnectionResult:
        def __init__(self, return_code, message=None):
            self.message = message
            self.return_code = return_code

        def __bool__(self):
            return self.return_code == 0

        def message(self):
            return self.message

    def __init__(self, ip='127.0.0.1', port=80, login=None, password=None):
        self.ip = ip
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
                f"{login}@{ip}" if login else ip,
                "sh", "-i"
            ]
        else:
            self.ssh_command = '/usr/bin/ssh'
            self.ssh_args = ["-t", f"-p{port}", ip, "sh", "-i"]
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
                print(ch, end='', file=sys.stderr)
            [print(line, file=sys.stdout, end='') for line in self.stdout.readlines()]
            if not is_process_running(pid):
                [print(line, file=sys.stderr, end='') for line in self.stderr.readlines()]
                [print(line, file=sys.stdout, end='') for line in self.stdout.readlines()]
                print("Worker process exited unexpectedly", file=sys.stderr)
                exit(1)

    def _wait_finish_marker(self, stdout_cb=None, stderr_cb=None, timeout=1):
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

    def sh(self, command, stdout=None, stderr=None):
        if not self._ping():
            print("ERROR")
            return None
        self.stdin.write(command)
        self.stdin.write("\n")

        def out_print(d):
            stdout.write(d)
            return True

        def err_print(d):
            stderr.write(d)
            return True

        if not self._wait_finish_marker(stdout_cb=out_print, stderr_cb=err_print):
            print("ERROR")
            return None
        [out_print(line) for line in self.stdout.readlines()]
        return self.ServerConnectionResult(0)
