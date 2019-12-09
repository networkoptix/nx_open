import logging
import platform
import subprocess
import sys
from io import StringIO

from vms_benchmark import exceptions

ini_ssh_command_timeout_s: int
ini_ssh_get_file_content_timeout_s: int


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

    def __init__(self, host, port, login, password, ssh_key):
        self.host = host
        target = f"{login}@{host}" if login else host
        if platform.system() == 'Linux':
            self.ssh_args = [
                *(('sshpass', '-p', password) if password else ()),
                'ssh',
                target, '-p', str(port),
                '-o', 'PubkeyAuthentication=' + ('no' if password else 'yes'),
                '-o', 'PasswordAuthentication=' + ('yes' if password else 'no'),
                '-o', 'StrictHostKeyChecking=no',
            ]
        else:
            self.ssh_args = [
                'plink',
                target, '-P', str(port),
                *(('-pw', password) if password else ()),
                '-batch',
            ]

        if ssh_key:
            self.ssh_args += ['-i', ssh_key]

        logging.info("SSH command:\n    " + '\n    '.join(self.ssh_args))

        self.ip = None
        self.local_ip = None
        self.is_root = False
        self.eth_name = None
        self.eth_speed = None

    def supply_host_key(self, host_key):
        assert self.ssh_args[0] == 'plink'  # ssh: StrictHostKeyChecking=no
        self.ssh_args += ['-hostkey', host_key]
        logging.info("SSH command:\n    " + '\n    '.join(self.ssh_args))

    def obtain_connection_info(self):
        ssh_connection_var_value = self.eval('echo $SSH_CONNECTION')
        ssh_connection_info = ssh_connection_var_value.strip().split() if ssh_connection_var_value else None
        if not ssh_connection_var_value or len(ssh_connection_info) < 3:
            raise exceptions.BoxCommandError(f'Unable to read SSH connection information.')

        self.ip = ssh_connection_info[2]
        self.local_ip = ssh_connection_info[0]
        self.is_root = self.eval('id -u') == '0'

        line_form_with_eth_name = self.eval(f'ip a | grep {self.ip}')
        eth_name = line_form_with_eth_name.split()[-1] if line_form_with_eth_name else None
        if not eth_name:
            raise exceptions.BoxCommandError(f'Unable to detect box network adapter which serves ip {self.ip}.')

        eth_dir = f'/sys/class/net/{eth_name}'
        eth_name_check_result = self.sh(f'test -d "{eth_dir}"')

        if not eth_name_check_result or eth_name_check_result.return_code != 0:
            raise exceptions.BoxCommandError(
                f'Unable to find box network adapter info dir {eth_dir!r}.'
            )

        self.eth_name = eth_name
        eth_speed = self.eval(f'cat /sys/class/net/{self.eth_name}/speed')
        self.eth_speed = eth_speed.strip() if eth_speed else None

    def sh(self, command, timeout_s=None,
           su=False, exc=False, stdout=sys.stdout, stderr=None, stdin=None):
        command_wrapped = command if self.is_root or not su else f'sudo -n {command}'

        logging.info(
            f"Executing remote command:\n"
            f"    {command_wrapped}\n"
            f"    stdin:\n"
            f"        {'        '.join(stdin.splitlines(keepends=True)) if stdin else 'N/A'}")

        actual_timeout_s = timeout_s or ini_ssh_command_timeout_s

        try:
            run = subprocess.run(
                [*self.ssh_args, command_wrapped],
                timeout=actual_timeout_s,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                input=stdin.encode() if stdin else None,
            )
        except subprocess.TimeoutExpired:
            message = f'Unable to execute remote command via ssh: timeout of {actual_timeout_s} seconds expired.'
            if exc:
                raise exceptions.BoxCommandError(message=message)
            else:
                return self.BoxConnectionResult(None, message, command=command_wrapped)

        logging.info(
            f"Remote command finished with exit status {run.returncode}\n"
            f"    stdout:\n"
            f"        {'        '.join(run.stdout.decode(errors='backslashreplace').splitlines(keepends=True))}\n"
            f"    stderr:\n"
            f"        {'        '.join(run.stderr.decode(errors='backslashreplace').splitlines(keepends=True))}")

        if self.ssh_args[0] == 'plink':
            if run.returncode == 0:  # Yes, exit status is 0 if access has been denied.
                if run.stderr.strip().lower() == 'access denied':
                    raise exceptions.BoxCommandError("SSH auth failed, check credentials")
            if run.returncode == 1:
                if run.stderr.strip().lower() == 'fatal error: network error: connection timed out':
                    raise exceptions.BoxCommandError(
                        "Connection timed out, "
                        "check boxHostnameOrIp configuration setting and "
                        "make sure that the box address is accessible")
                if run.stderr.strip().lower() == 'fatal error: network error: connection refused':
                    raise exceptions.BoxCommandError(
                        "Cannot connect via SSH, "
                        "check that SSH service is running "
                        "on port specified in boxSshPort .conf setting (22 by default)")
        else:
            if run.returncode == 255:
                if exc:
                    raise exceptions.BoxCommandError(message=run.stderr.decode('UTF-8').rstrip())
                else:
                    return self.BoxConnectionResult(
                        None,
                        run.stderr.decode('UTF-8').rstrip(), command=command_wrapped
                    )

        if stdout:
            stdout.write(run.stdout.decode())
            stdout.flush()
        if stderr:
            stderr.write(run.stderr.decode())
            stderr.flush()

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
