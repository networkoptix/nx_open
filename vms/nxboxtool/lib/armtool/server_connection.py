from os import pipe, fdopen, O_NONBLOCK, O_CLOEXEC, environ, fork, dup2, chdir, execvpe
from fcntl import fcntl, F_GETFL, F_SETFL, F_SETFD
import time
import sys

from pprint import pprint

class ServerConnection:
    class ServerConnectionResult:
        def __init__(self, message):
            self.message = message

        def __bool__(self):
            return False

        def message(self):
            return self.message

    def __init__(self, ip='127.0.0.1', port=80, login='root', password='qweasd234'):
        self.ip = ip
        self.port = port
        self.login = login
        self.password = password
        self.ssh_command = '/usr/bin/ssh'
        self.ssh_args = ["-T", f"-p{port}", ip, "sh", "-i"]
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

        time.sleep(1)

        self.stdin.write("stty -echo\n")
        self.stdin.write("PS1='\\03'\n")
        while True:
            time.sleep(0.001)
            ch = self.stderr.read(1)
            if str(ch) == str(chr(3)):
                break

    def _wait_finish_marker(self, stdout_cb=None, stderr_cb=None, timeout=1):
        started_at = time.time()
        right_sequence = ["\n", chr(3)]
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
            print(d, end='')
            return True

        if not self._wait_finish_marker(stdout_cb=out_print, stderr_cb=out_print):
            print("ERROR")
            return None
        return self.ServerConnectionResult("FOoo")
