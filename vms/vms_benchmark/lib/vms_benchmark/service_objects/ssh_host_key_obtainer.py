import logging
import re
from io import StringIO

from vms_benchmark.exceptions import SshHostKeyObtainingFailed, BoxCommandError


# plink (used on Windows) does not have an option to disable ssh host key checking. Instead, the
# only solution is to pass the key explicitly. Thus, on Windows we should preliminarily obtain the
# ssh host key.
class SshHostKeyObtainer:
    def __init__(self, dev, conf_file):
        self.dev = dev
        self.conf_file = conf_file

    def call(self):
        stderr = StringIO()
        res = self.dev.sh(command='true', stderr=stderr, verbose=True)
        error_messages = re.split(r'[\r\n]+', stderr.getvalue().strip())

        def fail(reason):
            logging.error(
                'Obtaining host key failed: %s. plink reported:\n%r',
                reason,
                error_messages
            )
            raise SshHostKeyObtainingFailed("Unable to obtain ssh host key of the box.")

        if error_messages:
            marker_message_index = None

            for i, message in enumerate(error_messages):
                if re.match(r'.*fingerprint is:$', message):
                    marker_message_index = i

            if marker_message_index is not None and marker_message_index+1 < len(error_messages):
                items = error_messages[marker_message_index+1].split()
                if len(items) == 0 or not items[-1]:
                    fail('stderr second-to-last line is blank')

                host_key = items[-1]

                return host_key

            fail('unexpected stderr output')

        if not res or res.return_code != 0:
            logging.error('Connecting via ssh failed: plink reported:\n%r', error_messages)
            raise BoxCommandError(
                'Unable to connect to the box via ssh; ' +
                f'check box credentials in {self.conf_file!r}'
            )

        logging.error(
            'Obtaining host key failed: plink executed successfully, but reported no messages.'
        )
        raise SshHostKeyObtainingFailed("Unable to obtain ssh host key of the box.")
