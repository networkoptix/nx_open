import logging
import time
import calendar
import pytz
import tzlocal
from datetime import datetime, timedelta


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

def datetime_to_str(date_time):
  return date_time.strftime('%Y-%m-%d %H:%M:%S.%f %Z')


class RunningTime(object):
    """Accounts time passed from creation and error due to network round-trip.
    >>> ours = RunningTime(datetime(2017, 11, 5, 0, 0, 10, tzinfo=pytz.utc), timedelta(seconds=4))
    >>> ours, str(ours)  # doctest: +ELLIPSIS
    (RunningTime(...2017-11-05...+/-...), '2017-11-05 ... +/- 2...')
    >>> ours.is_close_to(datetime(2017, 11, 5, 0, 0, 5, tzinfo=pytz.utc), threshold=timedelta(seconds=2))
    False
    >>> theirs = RunningTime(datetime(2017, 11, 5, 0, 0, 5, tzinfo=pytz.utc), timedelta(seconds=4))
    >>> ours.is_close_to(theirs, threshold=timedelta(seconds=2))
    True
    """

    def __init__(self, received_time, network_round_trip):
        half_network_round_trip = timedelta(seconds=network_round_trip.total_seconds() / 2)
        self._received_at = datetime.now(pytz.utc)
        self._initial = received_time + half_network_round_trip
        self._error = half_network_round_trip  # Doesn't change significantly during runtime.

    @property
    def _current(self):
        return self._initial + (datetime.now(pytz.utc) - self._received_at)

    def is_close_to(self, other, threshold=timedelta(seconds=2)):
        if isinstance(other, self.__class__):
            return abs(self._current - other._current) <= threshold + self._error + other._error
        else:
            return abs(self._current - other) <= threshold + self._error

    def __str__(self):
        return '{} +/- {}'.format(self._current.strftime('%Y-%m-%d %H:%M:%S.%f %Z'), self._error.total_seconds())

    def __repr__(self):
        return '{self.__class__.__name__}({self:s})'.format(self=self)


def wait_until(check_condition, timeout_sec=10):
    start_timestamp = time.time()
    delay_sec = 0.1
    while True:
        if check_condition():
            return True
        if time.time() - start_timestamp >= timeout_sec:
            return False
        time.sleep(delay_sec)
        delay_sec *= 2


def holds_long_enough(check_condition, timeout_sec=10):
    return not wait_until(lambda: not check_condition(), timeout_sec=timeout_sec)


