import base64
import json
import logging

from framework.os_access.windows_remoting.cmd import receive_stdout_and_stderr_until_done

log = logging.getLogger(__name__)


class PowershellError(Exception):
    def __init__(self, code, data):
        self.code = code
        self.data = data
        self.message = str(data[0]['Exception'])


def start_powershell_script(shell, script):
    return shell.start(
        'PowerShell',
        '-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Unrestricted',
        '-EncodedCommand', base64.b64encode(script.encode('utf_16_le')).decode('ascii'))


def run_powershell_command(shell, *command):
    error_handling_args = ['-ErrorVariable', 'exceptions', '-ErrorAction', 'SilentlyContinue']
    assign_result_to_variables = ['$output', '='] + list(command) + error_handling_args
    return_result = ['ConvertTo-Json', '$output', ',', '$exceptions']
    complete_command = assign_result_to_variables + ['^;'] + return_result
    log.getChild('ps.command').debug(' '.join(complete_command))
    with shell.start('PowerShell', *complete_command) as command:
        stdout, stderr = receive_stdout_and_stderr_until_done(command)
    log.getChild('ps.code').debug(command.exit_code)
    log.getChild('ps.stderr').debug(stderr.decode(errors='backslashreplace'))
    log.getChild('ps.stdout').debug(stdout.decode(errors='backslashreplace'))
    output, exceptions = json.loads(stdout.decode())  # There should be no errors, error otherwise.
    if command.exit_code != 0 or exceptions:
        raise PowershellError(command.exit_code, exceptions)
    return output
