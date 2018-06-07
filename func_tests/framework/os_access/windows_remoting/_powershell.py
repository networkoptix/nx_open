import base64
import json
import logging
from textwrap import dedent

from ._cmd import receive_stdout_and_stderr_until_done

_logger = logging.getLogger(__name__)


def start_raw_powershell_script(shell, script):
    _logger.debug("Run\n%s", script)
    return shell.start(
        'PowerShell',
        '-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Unrestricted',
        '-EncodedCommand', base64.b64encode(script.encode('utf_16_le')).decode('ascii'))


def _indent(string, spaces='    '):
    return string.replace('\n', '\n' + spaces)


def format_script(body, variables):
    parameters = []
    arguments = []
    for name, value in sorted(variables.items()):
        parameters.append('${name}'.format(name=name))
        arguments.append(
            "-{name}:(ConvertFrom-Json '{value}')".format(
                name=name,
                value=json.dumps(value, indent=4).replace("'", "''"),
                )
            )
    function_name = 'Execute-Script'
    # Stub but valid PowerShell script. PyCharm syntax highlight can be enabled.
    # language=PowerShell
    script = '''
        $ProgressPreference = 'SilentlyContinue'
        $ErrorActionPreference = 'Stop'
        function Do-Something <# Function name #> (<# Parameters #>) {
            <# Body #>
        }
        try {
            $Result = Do-Something <# Execution #>
            # @( $Result ) converts $null to empty array, single object to array with single element
            # and don't make an array nested.
            ConvertTo-Json @( 'success', @( $Result ) )
        } catch {
            $ExceptionInfo = @(
                $_.Exception.GetType().FullName,
                $_.CategoryInfo.Category.ToString(),
                $_.Exception.Message
            )
            ConvertTo-Json @( 'fail', $ExceptionInfo )
        }
        '''
    script = dedent(script).strip()
    script = script.replace('Do-Something <# Function name #>', function_name)
    script = script.replace('<# Parameters #>', ', '.join(parameters))
    script = script.replace('Do-Something <# Execution #>', _indent(
        ' `\n'.join([function_name] + arguments)
        if arguments else
        function_name,
        ' ' * 8))
    script = script.replace('<# Body #>', _indent(
        dedent(body).strip(),
        ' ' * 4))
    script = script.replace(' \n', '\n')

    return script


class PowershellError(Exception):
    def __init__(self, type_name, category, message):
        super(PowershellError, self).__init__('{}: {}'.format(type_name, message))
        self.type_name = type_name
        self.category = category
        self.message = message


# TODO: Rename to power_shell.
def run_powershell_script(shell, script, variables):
    with start_raw_powershell_script(shell, format_script(script, variables)) as command:
        stdout, _ = receive_stdout_and_stderr_until_done(command)
    outcome, data = json.loads(stdout.decode())
    if outcome == 'success':
        return data
    raise PowershellError(*data)
