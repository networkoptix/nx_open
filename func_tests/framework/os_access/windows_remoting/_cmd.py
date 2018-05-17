import base64
import logging

import xmltodict

log = logging.getLogger(__name__)


class Shell(object):
    def __init__(self, protocol):
        self._protocol = protocol
        self._shell_id = None

    def __enter__(self):
        self._shell_id = self._protocol.open_shell(codepage=65001)
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self._protocol.close_shell(self._shell_id)
        self._shell_id = None

    def start(self, command, *arguments):
        assert self._shell_id is not None, "Shell is not opened, use shell with `with` clause"
        return _Command(self._protocol, self._shell_id, command, *arguments)


class _Command(object):
    def __init__(self, protocol, shell_id, command, *arguments):
        self._protocol = protocol
        self._shell_id = shell_id
        self._command_id = None
        self._command = command
        self._arguments = arguments
        self.is_done = False
        self.exit_code = None

    def __enter__(self):
        # Rewrite with bigger MaxEnvelopeSize, currently hardcoded to 150k, while 8M needed.
        log.getChild('command').debug(' '.join([self._command] + list(self._arguments)))
        self._command_id = self._protocol.run_command(
            self._shell_id,
            self._command, self._arguments,
            console_mode_stdin=False,
            skip_cmd_shell=True)
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self._protocol.cleanup_command(self._shell_id, self._command_id)
        self._command_id = None

    def send_stdin(self, stdin_bytes, end=False):
        assert not self.is_done
        log.getChild('stdin.size').debug(len(stdin_bytes))
        if len(stdin_bytes) < 8192:
            log.getChild('stdin.data').debug(stdin_bytes.decode(errors='backslashreplace'))
        # noinspection PyProtectedMember
        rq = {
            'env:Envelope': self._protocol._get_soap_header(
                resource_uri='http://schemas.microsoft.com/wbem/wsman/1/windows/shell/cmd',
                action='http://schemas.microsoft.com/wbem/wsman/1/windows/shell/Send',
                shell_id=self._shell_id)
            }
        stream = rq['env:Envelope'].setdefault('env:Body', {}).setdefault('rsp:Send', {}).setdefault('rsp:Stream', {})
        stream['@Name'] = 'stdin'
        stream['@CommandId'] = self._command_id
        stream['#text'] = base64.b64encode(stdin_bytes)
        if end:
            stream['@End'] = 'true'
        self._protocol.send_message(xmltodict.unparse(rq))
        log.getChild('stdin').debug("Sent")

    def receive_stdout_and_stderr(self):
        assert not self.is_done
        log.getChild('stdout').debug("Receive")
        # noinspection PyProtectedMember
        stdout_chunk, stderr_chunk, exit_code, self.is_done = self._protocol._raw_get_command_output(
            self._shell_id, self._command_id)
        log.getChild('stdout.size').debug(len(stdout_chunk))
        if len(stdout_chunk) < 8192:
            log.getChild('stdout.data').debug(stdout_chunk.decode(errors='backslashreplace'))
        log.getChild('stderr.data').debug(stderr_chunk.decode(errors='backslashreplace'))
        if self.is_done:
            assert isinstance(exit_code, int)
            self.exit_code = exit_code
        else:
            assert exit_code == -1
        return stdout_chunk, stderr_chunk


def receive_stdout_and_stderr_until_done(command):
    stdout = bytearray()
    stderr = bytearray()
    while not command.is_done:
        stdout_chunk, stderr_chunk = command.receive_stdout_and_stderr()
        stdout += stdout_chunk
        stderr += stderr_chunk
    return bytes(stdout), bytes(stderr)


def run_command(shell, arguments, stdin_bytes=None):
    with shell.start(*arguments) as command:
        if stdin_bytes is not None:
            command.send_stdin(stdin_bytes, end=True)
        stdout_bytes, stderr_bytes = receive_stdout_and_stderr_until_done(command)
    return command.exit_code, stdout_bytes, stderr_bytes
