import json
import logging
from pprint import pformat

from framework.os_access.windows_remoting.cmd import Shell, receive_stdout_and_stderr_until_done
from framework.os_access.windows_remoting.cmd.powershell import run_powershell_command, start_powershell_script

log = logging.getLogger(__name__)


def test_powershell_command(pywinrm_protocol):
    with Shell(pywinrm_protocol) as shell:
        output = run_powershell_command(shell, 'Get-ChildItem', 'C:\\')
        log.debug(pformat(output))


def test_powershell_script(pywinrm_protocol):
    powershell_command = '''
        $x = 1111
        $y = $x * $x
        $y | ConvertTo-Json
        '''
    with Shell(pywinrm_protocol) as shell:
        with start_powershell_script(shell, powershell_command) as command:
            stdout, stderr = receive_stdout_and_stderr_until_done(command)
            assert json.loads(stdout) == 1234321
