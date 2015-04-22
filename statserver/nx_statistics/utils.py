#!/usr/bin/python
'''nx_statistics.utils -- Utilits
'''

from email.mime.text import MIMEText
from subprocess import Popen, PIPE

import logging
import subprocess

LOG_FORMAT = '[%(asctime)s] {%(pathname)s:%(lineno)d} %(levelname)s - %(message)s'

logFormatter = logging.Formatter(LOG_FORMAT)

logHandler = logging.StreamHandler()
logHandler.setLevel(logging.INFO)
logHandler.setFormatter(logFormatter)

SENDMAIL = '/usr/sbin/sendmail -t -oi'

def sendmail(sender, reciever, subject, text):
    msg = MIMEText(text)
    msg['From'] = sender
    msg['To'] = reciever
    msg['Subject'] = subject

    cmd = Popen(SENDMAIL.split(' '), stdin=PIPE, stdout=PIPE, stderr=PIPE)
    our, err = cmd.communicate(msg)
    if cmd.wait() != 0:
        raise OSError("Command '%s' has failed: %s" % (SENDMAIL, err))
    return out
