import time
import sys
import logging
from telnetlib import Telnet
from uuid import uuid1
from re import sub as re_sub
from socket import timeout as SocketTimeoutError

from vms_benchmark import exceptions, box_connection
from vms_benchmark.box_connection import BoxConnection

class BoxConnectionTelnet(BoxConnection):
    def __init__(self, host, port, login, password):
        if not password or not login:
            raise exceptions.BoxCommandError(
                f'Unable to read box connection information. '
                    f'Check boxLogin and boxPassword in vms_benchmark.conf.')

        self.host = host
        self.port = port
        self.login = login
        self.password = password

        logging.info(
            'Using Telnet to connect to the Box:\n'
                f"{' ' * 4}host: {self.host}\n"
                f"{' ' * 4}port: {self.port}\n"
                f"{' ' * 4}login: {self.login}\n"
                f"{' ' * 4}password: {self.password}\n")

        self.ip = None
        self.local_ip = None
        self.is_root = False
        self.eth_name = None
        self.eth_speed = None

    def _obtain_connection_addresses(self):
        tn = Telnet(self.host, self.port, box_connection.ini_box_command_timeout_s)
        tn_socket = tn.get_socket()
        self.ip = tn_socket.getpeername()[0]
        self.local_ip = tn_socket.getsockname()[0]

    @staticmethod
    def _remove_service_data_from_command_output(reply: str, tearline: str) -> str:
        # Remove the tearline from the end of the reply.
        reply = re_sub(rf'{tearline}(\s+|$)', '', reply)

        # Remove CSI sequences from the reply.
        reply = re_sub(r'\x1b\[.*?[\x40-\x7E]', '', reply)

        # There can be other ESC-sequences. If the problem appears, the regexes to strip them
        # out should be added.

        return reply.strip()

    def _execute_command_on_the_box(self, command: str, timeout_s: int) -> str:
        # Connect to the box and send our credentials.
        tn = Telnet(self.host, self.port, timeout_s)

        tn.read_until(b"login: ", timeout_s)
        tn.write(self.login.encode('ascii') + b"\n")
        tn.read_until(b"Password: ")
        tn.write(self.password.encode('ascii') + b"\n")

        # Send the main command with the necessary auxiliary commands.
        tearline = f"--- {uuid1()} ---"
        full_command_line = (
            "export TERM=xterm;"
            f"echo '{tearline}';"
            f"{command};"
            f"echo '{tearline}';"
            "exit\n"
        )
        tn.write(full_command_line.encode('ascii'))

        # Read the box reply data until the first tearline, after which the output of the main
        # command should start.
        found_regex_index, *_ = tn.expect([rf"\s+{tearline}\s+".encode('ascii')], timeout_s)

        # No tearline from the the first auxiliary `echo` command - something went wrong.
        if found_regex_index == -1:
            raise exceptions.BoxCommandError(
                "Fatal remote command execution error. "
                    "Check boxHostnameOrIp, boxLogin and boxPassword in vms_benchmark.conf and "
                    "make sure that the Box address is accessible.")

        # Read the main command output with the second tearline (empty string is a valid output!).
        found_regex_index, _, reply = tn.expect([rf"{tearline}(\s+|$)".encode('ascii')], timeout_s)

        # No tearline from the second auxiliary `echo` command - should be a main command timeout.
        if found_regex_index == -1:
            raise SocketTimeoutError()

        return self._remove_service_data_from_command_output(reply.decode('ascii'), tearline)

    def sh(self, command, timeout_s=None,
            su=False, throw_timeout_exception=False, stdout=sys.stdout, stderr=None, stdin=None,
            verbose=False):
        command_wrapped = command if self.is_root or not su else f'sudo -n {command}'
        actual_timeout_s = timeout_s or box_connection.ini_box_command_timeout_s

        logging.info(
            "Executing remote command:\n"
                f"{' ' * 4}{command_wrapped}")

        try:
            reply = self._execute_command_on_the_box(command_wrapped, actual_timeout_s)
        except ConnectionRefusedError:
            raise exceptions.BoxCommandError(
                "Cannot connect via Telnet, "
                    "check that Telnet service is running "
                    "on port specified in boxTelnetPort .conf setting "
                    "(23 by default).")
        except (EOFError, SocketTimeoutError):
            if throw_timeout_exception:
                raise exceptions.BoxCommandError(
                    "Connection timed out, "
                        "check boxHostnameOrIp configuration setting and "
                        "make sure that the box address is accessible.")
            else:
                return self.BoxConnectionResult(None, '')

        formatted_reply = ''.join([f"{' ' * 8}{line}" for line in reply.splitlines(keepends=True)])
        logging.info(
            "Remote command finished\n"
                f"{' ' * 4}reply:\n"
                f"{formatted_reply}\n")

        if stdout:
            stdout.write(reply)
            stdout.flush()

        # Since stdout and stderr are not separated in the output of the command executed via
        # telnet, we copy the same data to "stderr" and "stdout" parameters of the function.
        # Strictly speaking, it is not a correct behavior, but so far it is enough to process all
        # our cases correctly.
        # TODO: Resolve this problem more systematically - either don't use separate stderr and
        # stdout data in the higher-level functions (for both SSH and Telnet connection methods),
        # or find a way to collect data from these streams separately when using Telnet connection.
        if stderr:
            stderr.write(reply)
            stderr.flush()

        return self.BoxConnectionResult(0, command=command_wrapped)
