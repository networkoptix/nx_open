import base64
import logging

import xmltodict
from winrm.exceptions import WinRMTransportError

from framework.os_access.command import Command

_logger = logging.getLogger(__name__)


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
        return _WinRMCommand(self._protocol, self._shell_id, command, *arguments)


class _WinRMCommand(Command):
    def __init__(self, protocol, shell_id, command, *arguments):
        super(_WinRMCommand, self).__init__()
        self._protocol = protocol
        self._shell_id = shell_id
        self._command_id = None
        self._command = command
        self._arguments = arguments
        self.is_done = False

    def __enter__(self):
        # Rewrite with bigger MaxEnvelopeSize, currently hardcoded to 150k, while 8M needed.
        _logger.getChild('command').debug(' '.join([self._command] + list(self._arguments)))
        self._command_id = self._protocol.run_command(
            self._shell_id,
            self._command, self._arguments,
            console_mode_stdin=False,
            skip_cmd_shell=True)
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        try:
            self._protocol.cleanup_command(self._shell_id, self._command_id)
        except WinRMTransportError as e:
            _logger.exception("XML:\n%s", e.response_text)
            raise
        self._command_id = None

    def send(self, stdin_bytes, is_last=False):
        assert not self.is_done
        _logger.getChild('stdin.size').debug(len(stdin_bytes))
        if len(stdin_bytes) < 8192:
            _logger.getChild('stdin.data').debug(bytes(stdin_bytes).decode(errors='backslashreplace'))
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
        # TODO: Chunked stdin upload.
        stream['#text'] = base64.b64encode(stdin_bytes)
        sent_bytes = len(stdin_bytes)
        if is_last:
            stream['@End'] = 'true'
        self._protocol.send_message(xmltodict.unparse(rq))
        try:
            stdin_text = bytes(stdin_bytes).decode('ascii')
        except UnicodeDecodeError:
            _logger.getChild('stdin').debug("Sent:\n%r", stdin_bytes)
        else:
            _logger.getChild('stdin').debug("Sent:\n%s", stdin_text)
        return sent_bytes

    def receive(self, timeout_sec):
        # TODO: Support timeouts.
        assert not self.is_done
        _logger.getChild('stdout').debug("Receive")
        # noinspection PyProtectedMember
        stdout_chunk, stderr_chunk, exit_code, self.is_done = self._protocol._raw_get_command_output(
            self._shell_id, self._command_id)
        _logger.getChild('stdout.size').debug(len(stdout_chunk))
        if len(stdout_chunk) < 8192:
            _logger.getChild('stdout.data').debug(stdout_chunk.decode(errors='backslashreplace'))
        _logger.getChild('stderr.data').debug(stderr_chunk.decode(errors='backslashreplace'))
        if self.is_done:
            assert isinstance(exit_code, int)
        else:
            assert exit_code == -1
        return exit_code, stdout_chunk, stderr_chunk


def receive_stdout_and_stderr_until_done(self):
    raise NotImplementedError("Use Command.communicate")
