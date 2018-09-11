import base64
import logging
import xml.etree.ElementTree as ET
from contextlib import closing
from pprint import pformat
from subprocess import list2cmdline

import xmltodict
from winrm.exceptions import WinRMTransportError

from framework.os_access.command import Command, CommandOutcome, Run

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

    def start(self, args, logger=None):
        assert self._shell_id is not None, "Shell is not opened, use shell with `with` clause"
        return _WinRMCommand(self._protocol, self._shell_id, args, logger=logger or _logger)


class _WinRMCommandOutcome(CommandOutcome):
    def __init__(self, nt_status):
        self._nt_status = nt_status

    @property
    def is_intended_termination(self):
        # See: https://msdn.microsoft.com/en-us/library/cc704588.aspx
        # See: "NT STATUS" search results in Google
        return self._nt_status == 0xC000013A

    @property
    def code(self):
        return self._nt_status

    @property
    def comment(self):
        # See: https://msdn.microsoft.com/en-us/library/cc704588.aspx
        # See: "NT STATUS" search results in Google
        # TODO: Find or create exhaustive database of NT STATUS values.
        if self._nt_status == 0:
            name = 'STATUS_SUCCESS'
            comment = "The operation completed successfully."
        elif self._nt_status == 0xC000013A:
            name = 'STATUS_CONTROL_C_EXIT'
            comment = "{Application Exit by CTRL+C} The application terminated as a result of a CTRL+C."
        else:
            name = str(self._nt_status)
            comment = "See: https://msdn.microsoft.com/en-us/library/cc704588.aspx"
        return "{} ({})".format(name, comment)


class _WinRMRun(Run):
    def __init__(self, protocol, shell_id, args, logger):

        # Rewrite with bigger MaxEnvelopeSize, currently hardcoded to 150k, while 8M needed.
        self._protocol = protocol
        self._shell_id = shell_id
        self._logger = logger
        self._logger.getChild('command').debug(' '.join(args))

        req = {
            'env:Envelope': self._protocol._get_soap_header(
                resource_uri='http://schemas.microsoft.com/wbem/wsman/1/windows/shell/cmd',  # NOQA
                action='http://schemas.microsoft.com/wbem/wsman/1/windows/shell/Command',  # NOQA
                shell_id=shell_id)}

        # `WINRS_SKIP_CMD_SHELL` and `WINRS_CONSOLEMODE_STDIN` don't work.
        # See: https://social.msdn.microsoft.com/Forums/azure/en-US/d089b9b6-f7d3-439f-94f3-6e75497d113a
        # Commands are executed with `cmd /c`.
        # `cmd /c` has its own special (but simple) quoting rules. Thoroughly read output of `cmd /?`.
        # In short, quote normally, and then simply put quotes around.
        # It would fail only if complete script is a path that is very unlikely.

        req['env:Envelope']['env:Body'] = {
            'rsp:CommandLine': {
                'rsp:Command': '"' + list2cmdline(args) + '"',
                }
            }

        res = self._protocol.send_message(xmltodict.unparse(req))
        root = ET.fromstring(res)
        self._command_id = next(
            node for node in root.findall('.//*')
            if node.tag.endswith('CommandId')).text

        # TODO: Prohibit writes when passing up.
        self._is_done = False
        self._outcome = None

    @property
    def logger(self):
        return self._logger

    def send(self, stdin_bytes, is_last=False):
        # See: https://msdn.microsoft.com/en-us/library/cc251742.aspx.
        assert not self._is_done
        self._logger.getChild('stdin.size').debug(len(stdin_bytes))
        if len(stdin_bytes) < 8192:
            self._logger.getChild('stdin.data').debug(bytes(stdin_bytes).decode(errors='backslashreplace'))
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
            self._logger.getChild('stdin').debug("Sent:\n%r", stdin_bytes)
        else:
            self._logger.getChild('stdin').debug("Sent:\n%s", stdin_text)
        return sent_bytes

    @property
    def outcome(self):
        return self._outcome

    def receive(self, timeout_sec):
        # TODO: Support timeouts.
        if self._is_done:
            return None, None
        self._logger.getChild('stdout').debug("Receive.")
        # noinspection PyProtectedMember
        stdout_chunk, stderr_chunk, exit_code, is_done = self._protocol._raw_get_command_output(
            self._shell_id, self._command_id)
        assert isinstance(stdout_chunk, bytes)
        assert isinstance(stderr_chunk, bytes)
        assert isinstance(exit_code, int)
        assert isinstance(is_done, bool)
        if is_done:
            self._is_done = True
        else:
            assert not self._is_done
        self._logger.getChild('stdout.size').debug(len(stdout_chunk))
        if len(stdout_chunk) < 8192:
            self._logger.getChild('stdout.data').debug(stdout_chunk.decode(errors='backslashreplace'))
        self._logger.getChild('stderr.data').debug(stderr_chunk.decode(errors='backslashreplace'))
        if self._is_done:  # TODO: What if process is done but streams are not closed? Is that possible?
            assert 0 <= exit_code <= 0xFFFFFFFF
            self._outcome = _WinRMCommandOutcome(exit_code)
        else:
            assert exit_code == -1
        return stdout_chunk, stderr_chunk

    def _send_signal(self, signal):
        # See: https://msdn.microsoft.com/en-us/library/cc251743.aspx.
        rq = {
            'env:Envelope': self._protocol._get_soap_header(
                resource_uri='http://schemas.microsoft.com/wbem/wsman/1/windows/shell/cmd',
                action='http://schemas.microsoft.com/wbem/wsman/1/windows/shell/Signal',
                shell_id=self._shell_id)
            }
        rq['env:Envelope']['env:Body'] = {
            'rsp:Signal': {
                '@xmlns:rsp': 'http://schemas.microsoft.com/wbem/wsman/1/windows/shell',
                '@CommandId': self._command_id,
                'rsp:Code': 'http://schemas.microsoft.com/wbem/wsman/1/windows/shell/signal/' + signal,
                }
            }
        response = self._protocol.send_message(xmltodict.unparse(rq))
        self._logger.debug("Terminate response:\n%s", pformat(xmltodict.parse(response)))

    def terminate(self):
        self._send_signal('ctrl_c')

    def close(self):
        try:
            self._protocol.cleanup_command(self._shell_id, self._command_id)
        except WinRMTransportError as e:
            self._logger.exception("XML:\n%s", e.response_text)


class _WinRMCommand(Command):
    def __init__(self, protocol, shell_id, args, logger):
        self._protocol = protocol
        self._shell_id = shell_id
        self._args = args
        self._logger = logger

    def running(self):
        return closing(_WinRMRun(self._protocol, self._shell_id, self._args, self._logger))


def receive_stdout_and_stderr_until_done(self):
    raise NotImplementedError("Use Command.communicate")
