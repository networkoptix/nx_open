"""Pool -- containers with .get(key) method with additional functionality via decorator pattern"""
# TODO: Come up with better name.
import logging

logger = logging.getLogger(__name__)


class CachingPool(object):
    """pylru fits too, but explicit cache is clearer when debugging."""

    def __init__(self, pool, created_beforehand):
        self._pool = pool
        self._cache = created_beforehand.copy()

    def get(self, key):
        try:
            cached_value = self._cache[key]
            logger.info("From cache: %r.", cached_value)
            return cached_value
        except KeyError:
            self._cache[key] = new_value = self._pool.get(key)
            return new_value


class ClosingPool(object):
    def __init__(self, allocate, second_args, default_second_arg):  # TODO: Concise and flexible way of passing args.
        self._allocate = allocate
        self._second_args = second_args
        self._default_second_arg = default_second_arg
        self._entered = False
        self._entered_allocations = []

    def __enter__(self):
        assert not self._entered
        self._entered = True
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        assert self._entered
        while self._entered_allocations:
            resource, allocation = self._entered_allocations.pop()
            logger.info("Dispose: %r.", resource)
            allocation.__exit__(None, None, None)
        self._entered = False

    def get(self, key):
        assert self._entered
        allocation = self._allocate(key, self._second_args.get(key, self._default_second_arg))
        resource = allocation.__enter__()
        self._entered_allocations.append((resource, allocation))
        logger.info("Remember to dispose: %r.", resource)
        return resource
