import logging
import platform
import re
from io import StringIO

from vms_benchmark.exceptions import SshHostKeyObtainingFailed, BoxCommandError


# TODO: #alevenkov: Add a comment explaining why this is needed.
# TODO: Rename to ObtainSshHostKeyIfNeeded.
class SshHostKeyIsKnown:
    def __init__(self, dev, conf_file):
        self.dev = dev
        self.conf_file = conf_file

    def call(self):
        if platform.system() == 'Windows':
            stderr = StringIO()
            res = self.dev.sh(command='true', stderr=stderr)
            error_messages = re.split(r'[\r\n]+', stderr.getvalue().strip())

            def fail(reason):
                logging.error(
                    'Obtaining host key failed: %s. Ssh reported:\n%r',
                    reason,
                    error_messages
                )
                raise SshHostKeyObtainingFailed("Unable to obtain ssh host key of the box.")

            host_key = None

            if error_messages and error_messages[-1] == 'Connection abandoned.':
                if len(error_messages) < 2:
                    fail('insufficient output on stderr')
                items = error_messages[-2].split()
                if len(items) == 0 or not items[-1]:
                    fail('stderr second-to-last line is blank')
                host_key = items[-1]

            if not res or res.return_code != 0:
                logging.error('Connecting via ssh failed: ssh reported:\n%r', error_messages)
                raise BoxCommandError(
                    f'Unable to connect to the box via ssh; check box credentials in {repr(self.conf_file)}.')

            return host_key
        else:
            return None
