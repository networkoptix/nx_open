import logging
import time
import timeit

_logger = logging.getLogger(__name__)


class Wait(object):
    def __init__(self, until, timeout_sec=30, attempts_limit=100, log_continue=_logger.debug, log_stop=_logger.error):
        self._until = until
        self._timeout_sec = timeout_sec
        self._started_at = timeit.default_timer()
        self._last_checked_at = self._started_at
        self._attempts_limit = attempts_limit
        self._attempts_made = 0
        self.delay_sec = 0.5
        self.log_continue = log_continue
        self.log_stop = log_stop
        self.log_continue(
            "Start waiting until %s: %.1f sec, %d attempts.",
            self._until, self._timeout_sec, self._attempts_limit)

    def again(self):
        now = timeit.default_timer()
        since_start_sec = time.time() - self._started_at
        if since_start_sec > self._timeout_sec or self._attempts_made >= self._attempts_limit:
            self.log_stop(
                "Stop waiting until %s: %g/%g sec, %d/%d attempts.",
                self._until, since_start_sec, self._timeout_sec, self._attempts_made, self._attempts_limit)
            return False
        since_last_checked_sec = now - self._last_checked_at
        if since_last_checked_sec < self.delay_sec:
            self.log_continue(
                "Continue waiting (asked earlier) until %s: %.1f/%.1f sec, %d/%d attempts, delay %.1f sec.",
                self._until, since_start_sec, self._timeout_sec, self._attempts_made, self._attempts_limit, self.delay_sec)
            return True
        self._attempts_made += 1
        self.delay_sec *= 2
        self.log_continue(
            "Continue waiting until %s: %.1f/%.1f sec, %d/%d attempts, delay %.1f sec.",
            self._until, since_start_sec, self._timeout_sec, self._attempts_made, self._attempts_limit, self.delay_sec)
        self._last_checked_at = now
        return True

    def sleep(self):
        self.log_continue("Sleep for %.1f seconds" % self.delay_sec)
        time.sleep(self.delay_sec)


def retry_on_exception(func, exception_type, until, timeout_sec=10):
    wait = Wait(until, timeout_sec=timeout_sec)
    while True:
        try:
            return func()
        except exception_type:
            if not wait.again():
                raise
        wait.sleep()


class WaitTimeout(Exception):
    pass


def _description_from_func(func):
    try:
        object_bound_to = func.__self__
    except AttributeError:
        if func.__name__ == '<lambda>':
            raise ValueError("Cannot make description from lambda")
        return func.__name__
    if object_bound_to is None:
        raise ValueError("Cannot make description from unbound method")
    return '{func.__self__!r}.{func.__name__!s}'.format(func=func)


def wait_for_true(bool_func, description=None, timeout_sec=30):
    if description is None:
        description = _description_from_func(bool_func)
    wait = Wait(description, timeout_sec=timeout_sec)
    while True:
        result = bool_func()
        if result:
            return result
        if not wait.again():
            raise WaitTimeout("Cannot wait anymore until " + description)
        wait.sleep()


class NotPersistent(Exception):
    pass


def ensure_persistence(condition_is_true, description, timeout_sec=10):
    if description is None:
        description = _description_from_func(condition_is_true)
    wait = Wait(description, timeout_sec=timeout_sec, log_continue=_logger.debug, log_stop=_logger.info)
    while True:
        if not condition_is_true():
            raise NotPersistent("Have waited until " + description)
        if not wait.again():
            break
        wait.sleep()
