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

SUITMARK = '[' # all messages from a testsuit starts with it, other are tests' internal messages
FAILMARK = '[  FAILED  ]'
STARTMARK = '[ RUN      ]'
OKMARK = '[       OK ]'

READ_ONLY = select.POLLIN | select.POLLPRI | select.POLLHUP | select.POLLERR
READY = select.POLLIN | select.POLLPRI

ToSend = []

def email_notify(text):
    text['From'] = MailFrom
    text['To'] = MailTo
    smtp = SMTP('localhost')
    #smtp.set_debuglevel(1)
    smtp.sendmail(MailFrom, MailTo, text.as_string())
    smtp.quit()

def error_notify(lines, has_error, has_stranges):
    what = ("errors and strange messages" if has_error and has_stranges
        else 'errors' if has_error else 'strange mesages')
    msg = MIMEText.MIMEText(
        ("Some %s occured:\n\n" % what) +
        "\n".join(lines) +
        ("\n\n[%s]" % time.strftime("%x %X %Z"))
    )
    msg['Subject'] = "Autotest errors"
    email_notify(msg)

def ok_notify():
    msg = MIMEText.MIMEText("Autotest passed OK")
    msg['Subject'] = "Autotest OK"
    email_notify(msg)


def check_repeats(repeats):
    if repeats > 1:
        ToSend[-1] += "   [ REPEATS %s TIMES ]" % repeats


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
    last_suit_line = ''
    has_errors = False
    has_stranges = False
    repeats = 0 # now many times the same 'strange' line repeats
    while True:
        res = poller.poll(TIMEOUT)
        if res:
            event = res[0][1]
            if not(event & READY):
                break
            ch = proc.stdout.read(1)
            if ch == '\n':
                if len(line) > 0:
                    print line
                    if line.startswith(SUITMARK):
                        if line.startswith(FAILMARK):
                            ToSend.append(line) # line[len(FAILMARK):].strip())
                            last_suit_line = ''
                            has_errors = True
                        else:
                            last_suit_line = line
                    else: # gother test's messages
                        if last_suit_line != '':
                            ToSend.append(last_suit_line)
                            last_suit_line = ''
                        if ToSend and (line == ToSend[-1]):
                            repeats += 1
                        else:
                            check_repeats(repeats)
                            repeats = 1
                            ToSend.append(line)
                        has_stranges = True
                    line = ''
            else:
                line += ch
        else:
            check_repeats(repeats)
            print "TEST SUIT HAS TIMED OUT"
            ToSend.append("TEST SUIT HAS TIMED OUT")
            has_errors = True
            break
    print "---"
    if proc.poll() is None:
        proc.terminate()

    if proc.returncode:
        print "TEST SUIT RETURNS CODE %s" % proc.returncode
        check_repeats(repeats)
        ToSend.append("TEST SUIT RETURNS CODE %s" % proc.returncode)
        has_errors = True

    if ToSend:
        print "Interesting output:"
        for line in ToSend:
            print line
        print "Sending email..."
        error_notify(ToSend, has_errors, has_stranges)
    else:
        ok_notify()
        print "OK"


if __name__ == '__main__':
    run()