import logging
from collections import OrderedDict
from contextlib import contextmanager

from framework.move_lock import MoveLock
from framework.serialize import dump, load

logger = logging.getLogger(__name__)


class RegistryError(Exception):
    pass


class RegistryLimitReached(RegistryError):
    pass


class Registry(object):
    """Manage names allocation. Safe for parallel usage."""

    def __init__(self, ssh_access, path, name_format, limit):
        self._path = path
        self._lock = MoveLock(ssh_access, self._path.with_suffix('.lock'))
        if not self._path.parent.exists():
            logger.warning("Create %r; OK only if a clean run", self._path.parent)
            self._path.parent.mkdir(parents=True)
        with self._lock:
            self._path.write_bytes(b'')
        self._name_format = name_format
        self._limit = limit

    def __repr__(self):
        return '<Registry {}>'.format(self._path)

    @contextmanager
    def _reservations_locked(self):
        with self._lock:
            reservations = load(self._path.read_text())
            if reservations is None:
                reservations = OrderedDict()
            try:
                yield reservations
            except Exception:
                logger.warning("Exception thrown, don't save reservations in %r.", self)
                raise
            else:
                logger.info("Write reservations in %r.", self)
                self._path.write_text(dump(reservations))

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
                    logger.debug("%r: new %r.", self, name)
                    reservation = None
                else:
                    logger.debug("%r: %r -> %r.", self, name, reservation)
                if reservation is None:
                    logger.info("%r: %r taken with %r.", self, name, alias)
                    reservations[name] = alias
                    return index, name
        raise RegistryLimitReached("Cannot find vacant reservation in {} for {}".format(self, alias))

    def _free(self, name):
        with self._reservations_locked() as reservations:
            try:
                alias = reservations[name]
                if alias is None:
                    raise RegistryError("%r: %r is known but not reserved.", self, name)
                logger.info("%r: free %r.", name, self)
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
