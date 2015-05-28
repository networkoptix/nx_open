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
    '''sendmail simple wrapper
    '''
    msg = MIMEText(text)
    msg['From'] = sender
    msg['To'] = reciever
    msg['Subject'] = subject

    cmd = Popen(SENDMAIL.split(' '), stdin=PIPE, stdout=PIPE, stderr=PIPE)
    out, err = cmd.communicate(msg.as_string())
    if cmd.wait() != 0:
        raise OSError("Command '%s' has failed: %s" % (SENDMAIL, err))
    return out

class GeoIp2(object):
    '''geoip2 wrapper with cashe
    '''
    def __init__(self, db, service='country'):
        self._resolver = getattr(db, service)
        self._cache = {}

    def resolve(self, ip):
        if ip not in self._cache:
            loc = self._resolver(ip)
            self._cache[ip] = loc.country.iso_code
        return self._cache[ip]

    @staticmethod
    def local(path, service):
        from geoip2.database import Reader
        return GeoIp2(Reader(path), service)

    @staticmethod
    def remote(id, key, service):
        from geoip2.webservice import Client
        return GeoIp2(Client(id, key), service)

