import logging
import platform
import subprocess
import sys
from io import StringIO
from pathlib import Path

from vms_benchmark import exceptions, box_connection
from vms_benchmark.box_connection import BoxConnection

class BoxConnectionSSH(BoxConnection):
    connection_type_name = 'SSH'

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
                box_connection.ini_plink_bin,
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
        self.ssh_args += ['-hostkey', host_key]
        logging.info("SSH command:\n    " + '\n    '.join(self.ssh_args))

    def _obtain_connection_addresses(self):
        ssh_connection_var_value = self.eval('echo $SSH_CONNECTION')
        ssh_connection_info = ssh_connection_var_value.strip().split() if ssh_connection_var_value else None
        if not ssh_connection_var_value or len(ssh_connection_info) < 3:
            raise exceptions.BoxCommandError(
                f'Unable to read SSH connection information. '
                f'Check boxLogin and boxPassword in vms_benchmark.conf'
            )

        self.ip = ssh_connection_info[2]
        self.local_ip = ssh_connection_info[0]

    def sh(self, command, timeout_s=None,
            su=False, throw_exception_on_error=False, stdout=sys.stdout, stderr=None, stdin=None,
            verbose=False):
        command_wrapped = command if self.is_root or not su else f'sudo -n {command}'

        logging.info(
            f"Executing remote command:\n"
            f"    {command_wrapped}\n"
            f"    stdin:\n"
            f"        {'        '.join(stdin.splitlines(keepends=True)) if stdin else 'N/A'}")

        actual_timeout_s = timeout_s or box_connection.ini_box_command_timeout_s

        run_args = [*self.ssh_args]
        if verbose:
            run_args.append('-v')
        run_args.append(command_wrapped)

        try:
            run = subprocess.run(
                run_args,
                timeout=actual_timeout_s,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                input=stdin.encode() if stdin else None,
            )
        except subprocess.TimeoutExpired:
            message = f'Unable to execute remote command via ssh: timeout of {actual_timeout_s} seconds expired.'
            if throw_exception_on_error:
                raise exceptions.BoxCommandError(message=message)
            else:
                return self.BoxConnectionResult(None, message, command=command_wrapped)
        except FileNotFoundError as exception:
            raise exceptions.VmsBenchmarkError(
                f'Unable to find plink executable file: {run_args[0]!r}',
                original_exception=exception
            )
        except PermissionError as exception:
            raise exceptions.VmsBenchmarkError(
                f'Unable to run plink executable file: {run_args[0]!r}',
                original_exception=exception
            )

        logging.info(
            f"Remote command finished with exit status {run.returncode}\n"
            f"    stdout:\n"
            f"        {'        '.join(run.stdout.decode(errors='backslashreplace').splitlines(keepends=True))}\n"
            f"    stderr:\n"
            f"        {'        '.join(run.stderr.decode(errors='backslashreplace').splitlines(keepends=True))}")

        ssh_executable_path = Path(self.ssh_args[0])
        if ssh_executable_path.stem == 'plink':
            error_message = run.stderr.decode().strip().lower()
            if run.returncode == 0:  # Yes, exit status is 0 if access has been denied.
                if error_message == 'access denied':
                    raise exceptions.BoxCommandError("SSH auth failed, check credentials")
            if run.returncode == 1:
                if error_message == 'fatal error: network error: connection timed out':
                    raise exceptions.BoxCommandError(
                        "Connection timed out, "
                        "check boxHostnameOrIp configuration setting and "
                        "make sure that the box address is accessible")
                if error_message == 'fatal error: network error: connection refused':
                    raise exceptions.BoxCommandError(
                        "Cannot connect via SSH, "
                        "check that SSH service is running "
                        "on port specified in boxSshPort .conf setting (22 by default)")
        else:
            if run.returncode == 255:
                if throw_exception_on_error:
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
