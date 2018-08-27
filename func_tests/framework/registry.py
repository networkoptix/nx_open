import logging
from contextlib import contextmanager

from framework.lock import AlreadyAcquired
from framework.os_access.os_access_interface import OSAccess

_logger = logging.getLogger(__name__)


class RegistryError(Exception):
    pass


class RegistryLimitReached(RegistryError):
    pass


class Registry(object):
    """Manage names allocation. Safe for parallel usage."""

    def __init__(self, os_access, locks_dir, make_name, limit):
        self._os_access = os_access  # type: OSAccess
        self._dir = os_access.Path(locks_dir).expanduser()
        self._dir.mkdir(parents=True, exist_ok=True)
        self._make_name = make_name
        self._limit = limit

    def __str__(self):
        return 'Registry {}'.format(self._dir)

    def __repr__(self):
        return '<{!s}>'.format(self)

    def possible_entries(self):
        for index in range(1, self._limit + 1):
            name = self._make_name(index=index)
            yield index, name, self._dir / name

    @contextmanager
    def taken(self, alias):
        for index, name, lock_path in self.possible_entries():
            try:
                with self._os_access.lock(lock_path).acquired(timeout_sec=0):
                    _logger.info("%s: %s is taken with %r.", self, name, alias)
                    yield index, name
                    _logger.info("%s: %s is released from %r.", self, name, alias)
                    break
            except AlreadyAcquired:
                _logger.debug("%r: %s already acquired.", self, lock_path)
                continue
        else:
            raise RegistryLimitReached(repr(self))
