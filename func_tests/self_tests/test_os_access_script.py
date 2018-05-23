# test script for test_os_access.test_unclosed_stdout: finishes, but not closes stdout

import os, time, sys

sleep_time_sec = int(sys.argv[1])

if os.fork():
    print 'parent process, finished.'
else:
    print 'child process, sleeping for %d seconds...' % sleep_time_sec
    sys.stdout.flush()
    time.sleep(sleep_time_sec)
    print 'child process, finished.'
