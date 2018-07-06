import hashlib
import json
import logging
import time

from framework.os_access.windows_remoting.cmd import receive_stdout_and_stderr_until_done
from framework.os_access.windows_remoting.cmd.powershell import start_raw_powershell_script

log = logging.getLogger(__name__)


def read_bytes(shell, path):
    # language=PowerShell
    script_template = '''
        $ProgressPreference = 'SilentlyContinue'
        $File = [System.IO.File]::Open('{path}', [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read)
        $StandardOutput = [System.Console]::OpenStandardOutput()
        $File.CopyTo($StandardOutput)
        $StandardOutput.Close()
        $File.Close()
        '''
    data = bytearray()
    with start_raw_powershell_script(shell, script_template.format(path=path)) as command:
        started_at = time.time()
        while True:  # There must be at least one iteration: file may be empty.
            stdout, _ = command.receive_stdout_and_stderr()
            data += stdout
            kb_received = len(data) / 1024
            log.debug("Received %d kb (%d kb/s)", kb_received, kb_received / (time.time() - started_at))
            if command.is_done:
                break
    return bytes(data)


def write_bytes(shell, path, data, max_envelope_size_kb=8192):
    # language=PowerShell
    script_template = '''
        $ProgressPreference = 'SilentlyContinue'
        $StandardInput = [System.Console]::OpenStandardInput()
        $File = [System.IO.File]::Create('{path}')
        $StandardInput.CopyTo($File)
        $File.Close()
        $StandardInput.Close()
        Get-FileHash '{path}' -Algorithm MD5 | ConvertTo-Json
        '''
    max_chunk_size = int((max_envelope_size_kb - 64) / 1.5 * 1024)  # Size left for base64 in header.
    with start_raw_powershell_script(shell, script_template.format(path=path)) as command:
        bytes_sent = 0
        time_started = time.time()
        while True:  # There must be at least one iteration: file may be empty.
            chunk = data[bytes_sent:bytes_sent + max_chunk_size]
            completed = bytes_sent + len(chunk) == len(data)
            command.send_stdin(chunk, end=completed)
            bytes_sent += len(chunk)
            kb_sent = bytes_sent / 1024
            log.debug("Sent %d kb (%d kb/s)", kb_sent, kb_sent / (time.time() - time_started))
            if completed:
                break
        stdout, _ = receive_stdout_and_stderr_until_done(command)
        file_md5 = json.loads(stdout.strip())['Hash'].lower()
        data_md5 = hashlib.md5(data).hexdigest().lower()
        log.debug('File MD5: %s', file_md5)
        log.debug('Data MD5: %s', data_md5)
        assert file_md5 == data_md5
