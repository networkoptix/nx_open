import pprint
import time
import timeit

from typing import Any, Callable, Optional, Sequence, Union

from .context_logger import ContextLogger

_logger = ContextLogger(__name__, 'wait')


DEFAULT_MAX_DELAY_SEC = 5


class Wait(object):
    def __init__(self, until, timeout_sec=30, max_delay_sec=DEFAULT_MAX_DELAY_SEC, logger=None):
        # type: (str, float, float, ...) -> None
        self._until = until
        assert timeout_sec is not None
        self._timeout_sec = timeout_sec
        self._max_delay_sec = max_delay_sec
        self._started_at = timeit.default_timer()
        self._last_checked_at = self._started_at
        self._attempts_made = 0
        self.delay_sec = max_delay_sec / 16.
        self._logger = logger or _logger
        self._logger.info(
            "Waiting until %s: %.1f sec.",
            self._until, self._timeout_sec)

    def again(self):
        now = timeit.default_timer()
        since_start_sec = timeit.default_timer() - self._started_at
        if since_start_sec > self._timeout_sec:
            self._logger.warning(
                "Timed out waiting until %s: %g/%g sec, %d attempts.",
                self._until, since_start_sec, self._timeout_sec, self._attempts_made)
            return False
        since_last_checked_sec = now - self._last_checked_at
        if since_last_checked_sec < self.delay_sec:
            self._logger.debug(
                "Continue waiting (asked earlier) until %s: %.1f/%.1f sec, %d attempts, delay %.1f sec.",
                self._until, since_start_sec, self._timeout_sec, self._attempts_made, self.delay_sec)
            return True
        self._attempts_made += 1
        self.delay_sec = min(self._max_delay_sec, self.delay_sec * 2)
        self._logger.debug(
            "Continue waiting until %s: %.1f/%.1f sec, %d attempts, delay %.1f sec.",
            self._until, since_start_sec, self._timeout_sec, self._attempts_made, self.delay_sec)
        self._last_checked_at = now
        return True

    def sleep(self):
        self._logger.debug("Sleep for %.1f seconds" % self.delay_sec)
        time.sleep(self.delay_sec)


def retry_on_exception(
        func,
        exception_type,
        until,
        timeout_sec=10,
        max_delay_sec=DEFAULT_MAX_DELAY_SEC,
        logger=None,
        ):
    wait = Wait(until, timeout_sec, max_delay_sec, logger=logger)
    while True:
        try:
            return func()
        except exception_type:
            if not wait.again():
                raise
        wait.sleep()


class WaitTimeout(Exception):

    def __init__(self, timeout_sec, message):
        super(WaitTimeout, self).__init__(message)
        self.timeout_sec = timeout_sec


def _description_from_func(func):  # type: (Any) -> str
    try:
        object_bound_to = func.__self__
    except AttributeError:
        if func.__name__ == '<lambda>':
            raise ValueError("Cannot make description from lambda")
        return func.__name__
    if object_bound_to is None:
        raise ValueError("Cannot make description from unbound method")
    return '{func.__self__!r}.{func.__name__!s}'.format(func=func)


def wait_for_truthy(
        get_value,  # type: Callable[[], Any]
        description=None,  # type: Optional[str]
        timeout_sec=30,  # type: float
        max_delay_sec=DEFAULT_MAX_DELAY_SEC,  # type: float
        logger=None,
        ):
    if description is None:
        description = _description_from_func(get_value)
    wait = Wait(description, timeout_sec, max_delay_sec, logger=logger)
    while True:
        result = get_value()
        if result:
            return result
        if not wait.again():
            raise WaitTimeout(
                timeout_sec,
                "Timed out ({} seconds) waiting for: {}".format(timeout_sec, description),
                )
        wait.sleep()


def _get_by_path(composite_value, path):
    if composite_value is None:
        return None
    try:
        key, next_path = path[0], path[1:]
    except IndexError:
        return composite_value
    try:
        next_value = composite_value[key]
    except (KeyError, IndexError):
        _logger.debug("No %r in:\n%s", key, pprint.pformat(composite_value))
        return None
    return _get_by_path(next_value, next_path)


def wait_for_equal(
        get_actual,  # type: Callable[[], Any]
        expected,  # type: Any
        path=(),  # type: Sequence[Union[str, int]]
        actual_desc=None,  # type: Optional[str]
        expected_desc=None,  # type: Optional[str]
        timeout_sec=30,  # type: float
        max_delay_sec=DEFAULT_MAX_DELAY_SEC,  # type: float
        ):
    """
    @param path: If returned value is a big structure but only one value should be checked.
        Specify path to it in this parameter as a sequence of keys/indices:
        `('reply', 'remoteAddresses', 0)` or `('reply', 'systemInformation', 'platform')`.
    """
    if actual_desc is None:
        actual_desc = _description_from_func(get_actual)
    if expected_desc is None:
        expected_desc = repr(expected)
    if path:
        desc = "{} returns {} by path {}".format(actual_desc, expected_desc, path)
    else:
        desc = "{} returns {}".format(actual_desc, expected_desc)
    wait_for_truthy(
        lambda: _get_by_path(get_actual(), path) == expected,
        description=desc, timeout_sec=timeout_sec)


class NotPersistent(Exception):
    pass


def ensure_persistence(
        get_bool_value,  # type: Callable[[], bool]
        description,  # type: Optional[str]
        timeout_sec=10,  # type: float
        max_delay_sec=DEFAULT_MAX_DELAY_SEC,  # type: float
        logger=None,
        ):
    if description is None:
        description = _description_from_func(get_bool_value)
    wait = Wait(description, timeout_sec, max_delay_sec, logger=logger)
    while True:
        if not get_bool_value():
            raise NotPersistent("Have waited until " + description)
        if not wait.again():
            break
        wait.sleep()
