import calendar
import logging
import time
from datetime import datetime, timedelta

import pytz

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


class Timeout(Exception):
    pass


class Wait(object):
    def __init__(self, name, timeout_sec=10, attempts_limit=100, logging_levels=(logging.WARNING, logging.ERROR)):
        self._continue_level, self._stop_level = logging_levels
        self._name = name
        self._timeout_sec = timeout_sec
        self._started_at = time.time()
        self._attempts_limit = attempts_limit
        self._delay_sec = 0.1
        self._attempts_made = 0

    def sleep_and_continue(self):
        self._attempts_made += 1
        since_start_sec = time.time() - self._started_at
        if since_start_sec > self._timeout_sec or self._attempts_made >= self._attempts_limit:
            log.log(
                self._stop_level, "Stop to wait %s: %g/%g sec, %d/%d attempts.",
                self._name, since_start_sec, self._timeout_sec, self._attempts_made, self._attempts_limit)
            return False
        self._delay_sec *= 2
        log.log(
            self._continue_level, "Continue to wait %s: %g/%g sec, %d/%d attempts, sleep %g sec.",
            self._name, since_start_sec, self._timeout_sec, self._attempts_made, self._attempts_limit, self._delay_sec)
        time.sleep(self._delay_sec)
        return True


def wait_until(condition_is_true, name=None, timeout_sec=10):
    name = name or "until %s returns True" % condition_is_true.__name__
    wait = Wait(name=name, timeout_sec=timeout_sec)
    while True:
        if condition_is_true():
            break
        if not wait.sleep_and_continue():
            return False
    return True


def holds_long_enough(condition_is_true, name=None, timeout_sec=10):
    name = name or "while %s is returning True" % condition_is_true.__name__
    wait = Wait(name=name, timeout_sec=timeout_sec, logging_levels=(logging.DEBUG, logging.INFO))
    while True:
        if not condition_is_true():
            break
        if not wait.sleep_and_continue():
            return True
    return False


