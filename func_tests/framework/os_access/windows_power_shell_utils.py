import json
from textwrap import dedent

from pathlib2 import PurePath


def _indent(string, spaces='    '):
    return string.replace('\n', '\n' + spaces)


def power_shell_augment_script(body, variables):
    parameters = []
    arguments = []
    for name, value in sorted(variables.items()):
        parameters.append('${name}'.format(name=name))
        if isinstance(value, PurePath):
            value = str(PurePath)
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
