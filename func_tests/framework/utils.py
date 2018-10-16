import calendar
from functools import wraps
import inspect
import itertools
import logging
import socket
import struct
import threading
import time
from contextlib import closing
from datetime import datetime, timedelta
from multiprocessing.dummy import Pool as ThreadPool

import pytz
import tzlocal
from pylru import lrudecorator
from pytz import utc

_logger = logging.getLogger(__name__)


class SimpleNamespace:

    def __init__(self, **kwargs):
        self.__dict__.update(kwargs)

    def __repr__(self):
        keys = sorted(self.__dict__)
        items = ("{}={}".format(k, self.__dict__[k]) for k in keys)
        return "dict({})".format(type(self).__name__, ", ".join(items))

    def __eq__(self, other):
        return self.__dict__ == other.__dict__


def is_list_inst(l, cls):
    if type(l) is not list:
        return False
    for val in l:
        if not isinstance(val, cls):
            return False
    return True


def log_list(name, values):
    _logger.debug('%s:', name)
    for i, value in enumerate(values):
        _logger.debug('\t #%d: %s', i, value)


def flatten_list(list_of_lists):
    return list(itertools.chain(*tuple(list_of_lists)))

def quote(s, char='"'):
    return '%c%s%c' % (char, s, char)


def bool_to_str(val, false_str='false', true_str='true'):
    if val:
        return true_str
    else:
        return false_str


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


def datetime_local_now():
    return datetime.now(tz=tzlocal.get_localzone())


def datetime_to_str(date_time):
    return date_time.strftime('%Y-%m-%d %H:%M:%S.%f %Z')


class RunningTime(object):
    """Accounts time passed from creation and error due to network round-trip.
    >>> ours = RunningTime(datetime(2017, 11, 5, 0, 0, 10, tzinfo=pytz.utc), timedelta(seconds=4))
    >>> ours, str(ours)  # doctest: +ELLIPSIS
    (RunningTime(...2017-11-05...+/-...), '2017-11-05 ... +/- 2...')
    >>> ours.is_close_to(RunningTime(datetime(2017, 11, 5, 0, 0, 5, tzinfo=pytz.utc)), threshold=timedelta(seconds=2))
    False
    >>> theirs = RunningTime(datetime(2017, 11, 5, 0, 0, 5, tzinfo=pytz.utc), timedelta(seconds=4))
    >>> ours.is_close_to(theirs, threshold=timedelta(seconds=2))
    True
    """

    def __init__(self, received_time, network_round_trip=timedelta()):
        half_network_round_trip = timedelta(seconds=network_round_trip.total_seconds() / 2)
        self._received_at = datetime.now(pytz.utc)
        self._initial = received_time + half_network_round_trip
        self.error = half_network_round_trip  # Doesn't change significantly during runtime.

    @property
    def current(self):
        return self._initial + (datetime.now(pytz.utc) - self._received_at)

    def is_close_to(self, other, threshold=timedelta(seconds=2)):
        if isinstance(other, self.__class__):
            return abs(self.current - other.current) <= threshold + self.error + other.error
        else:
            return abs(self.current - other) <= threshold + self.error

    def __str__(self):
        return '{} +/- {}'.format(self.current.strftime('%Y-%m-%d %H:%M:%S.%f %Z'), self.error.total_seconds())

    def __repr__(self):
        return 'RunningTime({}, {})'.format(self.current, self.error * 2)


@lrudecorator(1)
def get_internet_time(address='time.rfc868server.com', port=37):
    """Get time from RFC868 time server wrap into Python's datetime.
    >>> import timeit
    >>> get_internet_time()  # doctest: +ELLIPSIS
    RunningTime(...)
    >>> timeit.timeit(get_internet_time, number=1) < 1e-4
    True
    """
    with closing(socket.socket(socket.AF_INET, socket.SOCK_STREAM)) as s:
        started_at = datetime.now(utc)
        s.connect((address, port))
        time_data = s.recv(4)
        request_duration = datetime.now(utc) - started_at
    remote_as_rfc868_timestamp, = struct.unpack('!I', time_data)
    posix_to_rfc868_diff = datetime.fromtimestamp(0, utc) - datetime(1900, 1, 1, tzinfo=utc)
    remote_as_posix_timestamp = remote_as_rfc868_timestamp - posix_to_rfc868_diff.total_seconds()
    remote_as_datetime = datetime.fromtimestamp(remote_as_posix_timestamp, utc)
    return RunningTime(remote_as_datetime, request_duration)


def with_traceback(fn, exception_event=None):
    @wraps(fn)  # critical for VMFactory.map to work
    def wrapper(*args, **kw):
        try:
            return fn(*args, **kw)
        except Exception:
            _logger.exception('Exception in %r:', fn)
            if exception_event:
                exception_event.set()
            raise
    return wrapper


def imerge(*iterators):
    for value_tuple in itertools.izip_longest(*iterators):
        for value in value_tuple:
            if value is not None:
                yield value


def single(iter):
    l = list(iter)
    assert len(l) == 1, 'Only single item is expected'
    return l[0]


def make_threaded_async_calls(thread_number, call_gen):
    failures = []

    def call(fn):
        try:
            fn()
        except:
            _logger.exception('Error calling %r:', fn)
            failures.append(None)

    pool = ThreadPool(thread_number)
    # convert generator to list to avoid generator-from-thread issues and races
    pool.map(call, list(call_gen))
    pool.close()
    pool.join()
    assert not failures, 'There was %d errors while generated method calls, check for logs' % len(failures)


class MultiFunction(object):

    def __init__(self, fn_list):
        self._fn_list = fn_list

    def __call__(self):
        for fn in self._fn_list:
            fn()


class FunctionWithDescription(object):

    def __init__(self, fn, description):
        self._fn = fn
        self.description = description

    def __call__(self, *args, **kw):
        return self._fn(args, **kw)


# based on: https://anandology.com/blog/using-iterators-and-generators/
class ThreadSafeIterator:

    def __init__(self, it):
        assert inspect.isgenerator(it), repr(it)
        self._it = it
        self._lock = threading.Lock()

    def __iter__(self):
        return self

    def next(self):
        with self._lock:
            return next(self._it)


def threadsafe_generator(generator_fn):

    def safe_generator_fn(*args, **kw):
        return ThreadSafeIterator(generator_fn(*args, **kw))

    return safe_generator_fn
