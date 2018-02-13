import email
import imaplib
import logging
import re
import time

import pytest

from .utils import datetime_utc_now

log = logging.getLogger(__name__)


ACTIVATION_EMAIL_SUBJECT = 'Activate your account'


class IMAPConnection(object):

    def __init__(self, hostname, email, password):
        assert email and password, repr((email, password))
        log.debug('\tIMAP: connecting to %r', hostname)
        self._connection = imaplib.IMAP4_SSL(hostname)
        self._call('login', email, password)
        self._call('select')

    def __enter__(self):
        return self

    def __exit__(self, type, value, traceback):
        self.close()

    def close(self):
        self._call('close')

    def expunge(self):
        self._call('expunge')

    # gmail drops '+xxx' part from emails when we load or search for 'To' headers,
    # so we have to search for subject and load all rfc822 headers to check for it
    def search_for_activation_emails(self, cloud_email):
        self._call('select')  # google wants this to refresh search base
        for uid, message in self._search('HEADER', 'Subject', ACTIVATION_EMAIL_SUBJECT):
            if message['To'] == cloud_email:
                yield Message(self, uid)

    def _call(self, fn_name, *args, **kw):
        fn = getattr(self._connection, fn_name)
        typ, dat = fn(*args, **kw)
        if fn_name == 'login':
            login, password = args
            args = (login, '***')  # mask password
        log.debug('\tIMAP: %s -> %r %r', ' '.join(str(arg) for arg in (fn_name,) + args), typ, dat)
        assert typ == 'OK', repr((typ, dat))
        return dat

    def _search(self, *args):
        response = self._call('uid', 'search', None, *args)
        # print 'message_id_list:', response
        if response[0] is None:
            return
        message_uid_list = sorted(response[0].split(), key=int, reverse=True)
        if not message_uid_list:
            return
        fetch_response = self._call('uid', 'fetch', ','.join(message_uid_list), '(RFC822.HEADER)')
        for line in fetch_response:
            if line == ')': continue
            uid_line, headers = line
            uid = self._parse_fetch_uid(uid_line)
            message = email.message_from_string(headers)  # with only headers
            yield (uid, message)

    def _parse_fetch_uid(self, uid_line):
        mo = re.search(r'\(UID (\w+)\W', uid_line)
        assert mo, repr(uid_line)  # Expected something like this: '1014 (UID 1020 RFC822.HEADER {3682}'
        return mo.group(1)

    def delete_old_activation_messages(self, cloud_email):
        old_messages = list(self.search_for_activation_emails(cloud_email))
        for message in old_messages:
            message.delete()
        if old_messages:
            self.expunge()

    def fetch_activation_code(self, cloud_host, cloud_email, timeout):
        start_time = datetime_utc_now()
        while datetime_utc_now() < start_time + timeout:
            for message in self.search_for_activation_emails(cloud_email):
                log.info('Found new message: %r', message)
                code = message.fetch_activation_code(cloud_host)
                if code:
                    log.info('Found activation code: %s', code)
                    message.delete()
                    self.expunge()
                    return code
            time.sleep(5)
        pytest.fail('Timed out in %r waiting for activation email to appear' % timeout)


class Message(object):

    def __init__(self, connection, uid):
        self._connection = connection
        self._uid = uid
        self._message = None

    def __str__(self):
        return '#%s' % self._uid

    def __repr__(self):
        return '<Message%s>' % self

    @property
    def message(self):
        if not self._message:
            response = self._connection._call('uid', 'fetch', self._uid, '(RFC822)')
            self._message = email.message_from_string(response[0][1])
        return self._message

    def delete(self):
        log.info('Deleting message %s', self)
        self._connection._call('uid', 'store', self._uid, '+FLAGS', r'(\Deleted)')

    def fetch_activation_code(self, cloud_host):
        for part in self.message.walk():
            if part.get_content_type() == 'text/plain':
                payload = part.get_payload(decode=True)
                # mo = re.search(r'https?://{}/activate/(\w+)'.format(cloud_host), payload)
                # Activation domain may not match account cloud host. Strange, yes.
                mo = re.search(r'https?://\S+/activate/(\w+)', payload)
                if mo:
                    return mo.group(1)
        return None
