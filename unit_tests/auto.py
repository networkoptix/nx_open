#!/usr/bin/env python
import sys, os, os.path, time
from subprocess import Popen, PIPE, STDOUT
import select
import traceback
from smtplib import SMTP
from email import MIMEText

TIMEOUT = 10 * 1000

RUNPATH = os.path.abspath('..') # what will be the subproceses' current directory
BINPATH = os.path.abspath("../build_environment/target/lib/release")
LIBPATH = BINPATH
TESTBIN = os.path.join(BINPATH, 'common_ut')

MailFrom = "autotest-script"
MailTo = 'dlavrentyuk@networkoptix.com'

FAILMARK = '[  FAILED  ]'

READ_ONLY = select.POLLIN | select.POLLPRI | select.POLLHUP | select.POLLERR
READY = select.POLLIN | select.POLLPRI

FailedTest = []

def email_notify(text):
    text['From'] = MailFrom
    text['To'] = MailTo
    smtp = SMTP('localhost')
    #smtp.set_debuglevel(1)
    smtp.sendmail(MailFrom, MailTo, text.as_string())
    smtp.quit()

def error_notify(errors):
    msg = MIMEText.MIMEText(
        "Errors occured:\n" +
        "\n".join("*** %s" % line for line in errors) +
        "\n[%s]" % time.strftime("%x %X %Z")
    )
    msg['Subject'] = "Autotest errors"
    email_notify(msg)

def ok_notify():
    msg = MIMEText.MIMEText("Autotest passed OK")
    msg['Subject'] = "Autotest OK"
    email_notify(msg)


def run():
    env = os.environ.copy()
    if env.get('LD_LIBRARY_PATH'):
        env['LD_LIBRARY_PATH'] += os.pathsep + LIBPATH
    else:
        env['LD_LIBRARY_PATH'] = LIBPATH
    poller = select.poll()
    proc = Popen([TESTBIN], bufsize=0, stdout=PIPE, stderr=STDOUT, cwd=RUNPATH, env=env, universal_newlines=True)
    #print "Test is started with PID", proc.pid
    poller.register(proc.stdout, READ_ONLY)
    line = ''
    while True:
        res = poller.poll(TIMEOUT)
        if res:
            event = res[0][1]
            if not(event & READY):
                break
            ch = proc.stdout.read(1)
            if ch == '\n':
                print line
 #                if 'RUN' in line:
                if line.startswith(FAILMARK):
                    FailedTest.append(line[len(FAILMARK):].strip())
                line = ''
            else:
                line += ch
        else:
            FailedTest.append("TEST SUIT HAS TIMED OUT")
            break
    print "---"
    if proc.poll() is None:
        proc.terminate()

    if proc.returncode:
        print "TEST SUIT RETURNS CODE %s" % proc.returncode
        FailedTest.append("TEST SUIT RETURNS CODE %s" % proc.returncode)

    if FailedTest:
        print "ERRORS:"
        for line in FailedTest:
            print "*** %s" % line
        print "Sending email..."
        error_notify(FailedTest)
    else:
        ok_notify()
        print "OK"


if __name__ == '__main__':
    run()