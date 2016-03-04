# -*- coding: utf-8 -*-
"File based interprocess Lock class."

import time
from fcntl import flock, LOCK_EX, LOCK_UN, LOCK_NB


class Lock(object):
    def __init__(self, filename):
        self.filename = filename
        # This will create it if it does not exist already
        self.handle = open(filename, 'w')

    def acquire(self, timeout=None, sleep_quantum=0.1):
        """Nonblocking lock acquire method.
            timeout = None  - wait endlessly
            timeout = 0     - check and return immediately
            timeout > 0     - wait no more then timeout seconds
           If timeout is not None, sleep_quantum is a sleeping period
           between checks.
        """
        if timeout is None:
            try:
               flock(self.handle, LOCK_EX)
            except Exception:
               return False
            return True

        end = 0 if timeout == 0 else time.time() + timeout
        while True:
            try:
                flock(self.handle, LOCK_EX|LOCK_NB)
                return True
            except IOError:
                pass
            if end < time.time():
                return False
            time.sleep(sleep_quantum)

    def release(self):
        flock(self.handle, LOCK_UN)

    def __del__(self):
        self.handle.close()

