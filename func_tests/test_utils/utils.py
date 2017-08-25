import logging
import time
import calendar
import pytz
import tzlocal
from datetime import datetime


log = logging.getLogger(__name__)


class SimpleNamespace:

    def __init__(self, **kwargs):
        self.__dict__.update(kwargs)

    def __repr__(self):
        keys = sorted(self.__dict__)
        items = ("{}={!r}".format(k, self.__dict__[k]) for k in keys)
        return "{}({})".format(type(self).__name__, ", ".join(items))

    def __eq__(self, other):
        return self.__dict__ == other.__dict__


class GrowingSleep(object):

    _delay_levels = [10, 20, 30, 60]  # seconds
    _calls_per_level = 10

    def __init__(self):
        self._level = 0
        self._level_call_count = 0

    def sleep(self):
        time.sleep(self._delay_levels[self._level])
        self._level_call_count += 1
        if self._level_call_count >= self._calls_per_level and self._level < len(self._delay_levels) - 1:
            self._level += 1
            self._level_call_count = 0


def is_list_inst(l, cls):
    if type(l) is not list:
        return False
    for val in l:
        if not isinstance(val, cls):
            return False
    return True

def log_list(name, values):
    log.debug('%s:', name)
    for i, value in enumerate(values):
        log.debug('\t #%d: %s', i, value)

def quote(s, char='"'):
    return '%c%s%c' % (char, s, char)

def bool_to_str(val, false_str = 'false', true_str = 'true'):
    if val: return true_str
    else: return false_str

def str_to_bool(val):
    v = val.lower()
    if val == 'false' or val == '0':
        return False
    elif val == 'true' or val == '1':
        return True
    raise Exception('Invalid boolean "%s"' % val)

def datetime_utc_to_timestamp(date_time):
    return calendar.timegm(date_time.utctimetuple()) + date_time.microsecond/1000000.

def datetime_utc_from_timestamp(timestamp):
    return datetime.utcfromtimestamp(timestamp).replace(tzinfo=pytz.utc)

def datetime_utc_now():
    return datetime.utcnow().replace(tzinfo=pytz.utc)

def as_local_tz(dt):
    tz = tzlocal.get_localzone()
    return dt.astimezone(tz)

def datetime_to_str(date_time):
  return date_time.strftime('%Y-%m-%d %H:%M:%S.%f %Z')

