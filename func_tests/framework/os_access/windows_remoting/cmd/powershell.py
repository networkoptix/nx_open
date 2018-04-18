import base64
import json
import logging
from textwrap import dedent

from framework.os_access.windows_remoting.cmd import receive_stdout_and_stderr_until_done

log = logging.getLogger(__name__)


def start_raw_powershell_script(shell, script):
    log.debug("Run\n%s", script)
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
    script = dedent(
        '''
        $ProgressPreference = 'SilentlyContinue'
        $ErrorActionPreference = 'Stop'
        function {function_name} ({parameters}) {{
            {body}
        }}
        try {{
            $Result = {execution}
            # @( $Result ) converts $null to empty array, single object to array with single element
            # and don't make an array nested.
            ConvertTo-Json @( 'success', @( $Result ) )
        }} catch {{
            $ExceptionInfo = @(
                $_.Exception.GetType().FullName,
                $_.CategoryInfo.Category.ToString(),
                $_.Exception.Message
            )
            ConvertTo-Json @( 'fail', $ExceptionInfo )
        }}
        ''').strip().format(
        function_name=function_name,
        parameters=', '.join(parameters),
        execution=_indent(
            ' `\n'.join([function_name] + arguments)
            if arguments else
            function_name,
            ' ' * 8),
        body=_indent(
            dedent(body).strip(),
            ' ' * 4),
        ).replace(' \n', '\n')

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
