import logging
from collections import OrderedDict
from contextlib import contextmanager
from pprint import pformat

from framework.move_lock import MoveLock
from framework.serialize import dump, load

_logger = logging.getLogger(__name__)


class RegistryError(Exception):
    pass


class RegistryLimitReached(RegistryError):
    pass


class Registry(object):
    """Manage names allocation. Safe for parallel usage."""

    def __init__(self, ssh_access, path, name_format, limit):
        self._path = path
        self._lock = MoveLock(ssh_access, self._path.with_suffix('.lock'))
        self._path.parent.mkdir(parents=True, exist_ok=True)
        self._name_format = name_format
        self._limit = limit

    def __repr__(self):
        return '<Registry {}>'.format(self._path)

    def _read_reservations(self):
        if self._path.exists():
            reservations_raw = self._path.read_text()
            reservations = load(reservations_raw)
            if reservations is not None:
                _logger.debug("Read from %r:\n%s", self, pformat(reservations))
                return reservations
        _logger.debug("Empty or non-existent %r.", self)
        return OrderedDict()

    def _write_reservations(self, reservations):
        _logger.debug("Write to %r:\n%s", self, pformat(reservations))
        self._path.write_text(dump(reservations))

    @contextmanager
    def _reservations_locked(self):
        with self._lock:
            try:
                reservations = self._read_reservations()
                yield reservations
            except Exception:
                _logger.warning("Exception thrown, don't save reservations in %r.", self)
                raise
            else:
                self._write_reservations(reservations)

    def _make_name(self, index):
        digits = max(len(str(self._limit)), 3)
        index_str = str(index).zfill(digits)
        return self._name_format.format(index=index_str)

    def _take(self, alias):  # TODO: Rename alias: it's used merely as reminder or comment in file.
        with self._reservations_locked() as reservations:
            for index, name in self.possible_entries():
                try:
                    reservation = reservations[name]
                except KeyError:
                    _logger.debug("%r: new %r.", self, name)
                    reservation = None
                else:
                    _logger.debug("%r: %r -> %r.", self, name, reservation)
                if reservation is None:
                    _logger.info("%r: %r taken with %r.", self, name, alias)
                    reservations[name] = alias
                    return index, name
        raise RegistryLimitReached("Cannot find vacant reservation in {} for {}".format(self, alias))

    def _free(self, name):
        with self._reservations_locked() as reservations:
            try:
                alias = reservations[name]
                if alias is None:
                    raise RegistryError("%r: %r is known but not reserved.", self, name)
                _logger.info("%r: free %r.", self, name)
                reservations[name] = None
            except KeyError:
                raise RegistryError("%r: %r is not even known.", self, name)

    def possible_entries(self):
        for index in range(1, self._limit + 1):
            yield index, self._make_name(index)

    @contextmanager
    def taken(self, alias):
        index, name = self._take(alias)
        try:
            yield index, name
        finally:
            self._free(name)

    def for_each(self, procedure):
        with self._reservations_locked() as reservations:
            for name, alias in reservations.items():
                procedure(name, alias)
