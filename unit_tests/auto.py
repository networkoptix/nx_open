#!/usr/bin/env python
import sys, os, os.path, time
import subprocess
from subprocess import Popen, PIPE, STDOUT
import select
import signal
import traceback

TIMEOUT = 5 * 1000

RUNPATH = os.path.abspath('..') # what will be the subproceses' current directory
BINPATH = os.path.abspath("../build_environment/target/lib/release")
LIBPATH = BINPATH
TESTBIN = os.path.join(BINPATH, 'common_ut')

FAILMARK = '[  FAILED  ]'

READ_ONLY = select.POLLIN | select.POLLPRI | select.POLLHUP | select.POLLERR
READY = select.POLLIN | select.POLLPRI

def run():
    env = os.environ.copy()
    if env.get('LD_LIBRARY_PATH'):
        env['LD_LIBRARY_PATH'] += os.pathsep + LIBPATH
    else:
        env['LD_LIBRARY_PATH'] = LIBPATH
    poller = select.poll()
    proc = Popen([TESTBIN], bufsize=0, stdout=PIPE, stderr=STDOUT, cwd=RUNPATH, env=env, universal_newlines=True)
#    proc = Popen(['/bin/ls', '-l', '/home/danil'], bufsize=0, stdout=PIPE, stderr=STDOUT, cwd=RUNPATH, env=env, universal_newlines=True)
    print "Test is started with PID", proc.pid
    poller.register(proc.stdout, READ_ONLY)
    line = ''
    while True:
        res =  poller.poll(TIMEOUT)
        if res:
            event = res[0][1]
            if not(event & READY):
                break
            ch = proc.stdout.read(1)
            if ch == '\n':
#                print line
#                if 'RUN' in line:
#                    print line
                if line.startswith(FAILMARK):
                    print "*** FAIL: %s" % line[len(FAILMARK):]
                line = ''
            else:
                line += ch
        else:
            print "*** TIMED OUT ***"
            break
    if proc.poll() is None:
        proc.terminate()
    else:
        print "Test process terminated with %s code" % proc.returncode

if __name__ == '__main__':
    run()