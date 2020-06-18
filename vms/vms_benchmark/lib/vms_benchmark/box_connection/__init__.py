import sys
from io import StringIO
from enum import Enum

from vms_benchmark import exceptions

ini_plink_bin: str
ini_box_command_timeout_s: int
ini_box_get_file_content_timeout_s: int


class BoxConnection:
    class BoxConnectionResult:
        def __init__(self, return_code, message=None, command=None):
            self.message = message
            self.return_code = return_code
            self.command = command

        def __bool__(self):
            return self.return_code == 0

        def message(self):
            return self.message

    class ConnectionType(Enum):
        TELNET = 1
        SSH = 2

    @classmethod
    def create_box_connection_object(cls,
        host, port, login, password, connection_type=ConnectionType.SSH, ssh_key=None):

        if connection_type == cls.ConnectionType.SSH:
            return BoxConnectionSSH(host, port, login, password, ssh_key)
        elif connection_type == cls.ConnectionType.TELNET:
            return BoxConnectionTelnet(host, port, login, password)

        return None

    def __init__(self, host, port, login, password): # Should be overridden in subclass
        raise NotImplementedError('Generic "BoxConnection" class can\'t be instantiated!')

    def _obtain_connection_addresses(self):  # Should be overridden in subclass
        raise NotImplementedError('You have to define "_obtain_connection_addresses" method!')

    def obtain_connection_info(self):
        self._obtain_connection_addresses()
        self.is_root = self.eval('id -u') == '0'

        line_form_with_eth_name = self.eval(f'ip a | grep {self.ip}')
        eth_name = line_form_with_eth_name.split()[-1] if line_form_with_eth_name else None
        if not eth_name:
            # Some systems don't have the `ip` command - try to use `ifconfig`.
            line_form_with_eth_name = self.eval(f'ifconfig | grep -B1 {self.ip} | head -n1')
            eth_name = line_form_with_eth_name.split()[0] if line_form_with_eth_name else ''

        if not eth_name:
            raise exceptions.BoxCommandError(
                f'Unable to detect box network adapter which serves ip {self.ip}.')

        # The colon can appear as the part of the network interface name if it is an alias, or
        # in the end of the network interface name as a separator. Anyway, we have to remove
        # everything from the colon to the end of the string to get the interface name
        # suitable for furhter checks.
        eth_name = eth_name.split(':', 1)[0]
        self.eth_name = eth_name

        eth_dir = f'/sys/class/net/{eth_name}'
        eth_name_check_result = self.sh(f'test -d "{eth_dir}"')

        if not eth_name_check_result or eth_name_check_result.return_code != 0:
            raise exceptions.BoxCommandError(
                f'Unable to find box network adapter info dir {eth_dir!r}.'
            )

        eth_speed = self.eval(f'cat /sys/class/net/{self.eth_name}/speed')
        self.eth_speed = eth_speed.strip() if eth_speed else None

    def sh(self, command, timeout_s=None,
            su=False, throw_timeout_exception=False, stdout=sys.stdout, stderr=None, stdin=None,
            verbose=False):
        """ Should be overridden in subclass """
        raise NotImplementedError('You have to define "sh" method!')

    def eval(self, cmd, timeout_s=None, su=False, stderr=None, stdin=None):
        out = StringIO()
        res = self.sh(cmd, su=su, stdout=out, stderr=stderr, stdin=stdin, timeout_s=timeout_s)

        if res.return_code is None:
            raise exceptions.BoxCommandError(res.message)

        if not res:
            return None

        return out.getvalue().strip()

    def get_file_content(self, path, su=False, stderr=None, stdin=None, timeout_s=None):
        return self.eval(
            f'cat "{path}"', su=su, stderr=stderr, stdin=stdin,
            timeout_s=timeout_s or ini_box_get_file_content_timeout_s)


from .ssh import BoxConnectionSSH
from .telnet import BoxConnectionTelnet
