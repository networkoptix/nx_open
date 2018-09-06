import base64
import json
import logging

from framework.os_access.windows_power_shell_utils import power_shell_augment_script

_logger = logging.getLogger(__name__)


def start_raw_powershell_script(shell, script, logger=None):
    _logger.debug("Run\n%s", script)
    return shell.start([
        'PowerShell',
        '-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Unrestricted',
        '-EncodedCommand', base64.b64encode(script.encode('utf_16_le'),).decode('ascii'),
        ], logger=logger)


class PowershellError(Exception):
    def __init__(self, type_name, category, message):
        super(PowershellError, self).__init__('{}: {}'.format(type_name, message))
        self.type_name = type_name
        self.category = category
        self.message = message


# TODO: Rename to power_shell.
def run_powershell_script(shell, script, variables, logger=None):
    with start_raw_powershell_script(
            shell,
            power_shell_augment_script(script, variables),
            logger=logger,
            ).running() as run:
        stdout, _ = run.communicate()
    outcome, data = json.loads(stdout.decode())
    if outcome == 'success':
        return data
    raise PowershellError(*data)
