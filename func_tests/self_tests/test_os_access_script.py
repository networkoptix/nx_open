"""Forked child outlives parent

Parent process forks and exits.
Child inherits and holds its stdout and stderr streams.

Script used in for test_os_access.test_unclosed_stdout.
"""
from __future__ import print_function

import os
import sys
import time

if __name__ == '__main__':
    sleep_time_sec = int(sys.argv[1])

    if os.fork():
        print('Parent process, finished.')
    else:
        print('Child process, sleeping for {:d} seconds...'.format(sleep_time_sec))
        sys.stdout.flush()
        time.sleep(sleep_time_sec)
        print('Child process, finished.')
