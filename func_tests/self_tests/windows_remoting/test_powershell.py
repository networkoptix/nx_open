import json
import logging
from textwrap import dedent

import pytest

from framework.os_access.windows_remoting._cmd import Shell, receive_stdout_and_stderr_until_done
from framework.os_access.windows_remoting._powershell import (
    PowershellError,
    format_script,
    run_powershell_script,
    start_raw_powershell_script,
    )

log = logging.getLogger(__name__)


@pytest.fixture()
def shell(pywinrm_protocol):
    with Shell(pywinrm_protocol) as shell:
        yield shell


def test_start_script(pywinrm_protocol):
    # language=PowerShell
    powershell_command = '''
        $x = 1111
        $y = $x * $x
        $y | ConvertTo-Json
        '''
    with Shell(pywinrm_protocol) as shell:
        with start_raw_powershell_script(shell, powershell_command) as command:
            stdout, stderr = receive_stdout_and_stderr_until_done(command)
            assert json.loads(stdout.decode()) == 1234321


def test_format_script():
    # Variable names are sorted to get predictable result.
    # language=PowerShell
    body = '''
        Do-This -Carefully $BThing
        Make-It -Real -Quality $AQuality
        '''
    variables = {'AQuality': 100, 'BThing': ['Something', 'Another']}
    # language=PowerShell
    expected_formatted_script = dedent('''
        $ProgressPreference = 'SilentlyContinue'
        $ErrorActionPreference = 'Stop'
        function Execute-Script ($AQuality, $BThing) {
            Do-This -Carefully $BThing
            Make-It -Real -Quality $AQuality
        }
        try {
            $Result = Execute-Script `
                -AQuality:(ConvertFrom-Json '100') `
                -BThing:(ConvertFrom-Json '[
                    "Something",
                    "Another"
                ]')
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
        ''').strip()
    assert format_script(body, variables) == expected_formatted_script


def test_run_script(shell):
    result = run_powershell_script(
        shell,
        # language=PowerShell
        '''
            $a = $x * $x
            $b = $a + $y
            return $b
            ''',
        {'x': 2, 'y': 5})
    assert result == [9]


def test_run_script_error(shell):
    non_existing_group = 'nonExistingGroup'
    with pytest.raises(PowershellError) as exception_info:
        _ = run_powershell_script(
            shell,
            # language=PowerShell
            '''Get-LocalGroup -Name:$Group''',
            {'Group': non_existing_group})
    exception = exception_info.value
    assert exception.message == 'Group nonExistingGroup was not found.'
    assert exception.category == 'ObjectNotFound'
