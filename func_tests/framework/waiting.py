import time
import timeit

from framework.utils import log


class Wait(object):
    def __init__(self, until, timeout_sec=30, attempts_limit=100, log_continue=log.debug, log_stop=log.error):
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
            "Start waiting %s: %.1f sec, %d attempts.",
            self._until, self._timeout_sec, self._attempts_limit)

    def again(self):
        now = timeit.default_timer()
        if now - self._last_checked_at < self.delay_sec:
            return True
        self._attempts_made += 1
        self.delay_sec *= 2
        since_start_sec = time.time() - self._started_at
        if since_start_sec > self._timeout_sec or self._attempts_made >= self._attempts_limit:
            self.log_stop(
                "Stop waiting until %s: %g/%g sec, %d/%d attempts.",
                self._until, since_start_sec, self._timeout_sec, self._attempts_made, self._attempts_limit)
            return False
        self.log_continue(
            "Continue waiting until %s: %.1f/%.1f sec, %d/%d attempts, delay %.1f sec.",
            self._until, since_start_sec, self._timeout_sec, self._attempts_made, self._attempts_limit, self.delay_sec)
        self._last_checked_at = now
        return True

    def sleep(self):
        time.sleep(self.delay_sec)

    def sleep_and_continue(self):
        if self.again():
            self.sleep()
            return True
        return False


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


def wait_for_true(bool_func, description, timeout_sec=30):
    wait = Wait(description, timeout_sec=timeout_sec)
    while True:
        if bool_func():
            break
        if not wait.again():
            raise WaitTimeout("Cannot wait anymore until " + description)
        wait.sleep()


class NotPersistent(Exception):
    pass


def ensure_persistence(condition_is_true, description, timeout_sec=10):
    wait = Wait(description, timeout_sec=timeout_sec, log_continue=log.debug, log_stop=log.info)
    while True:
        if not condition_is_true():
            raise NotPersistent("Have waited until " + description)
        if not wait.again():
            break
        wait.sleep()
