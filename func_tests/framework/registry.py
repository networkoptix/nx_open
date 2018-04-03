import logging
from pprint import pformat

from framework.move_lock import MoveLock
from framework.os_access import FileNotFound
from framework.serialize import dump, load

logger = logging.getLogger(__name__)


class Registry(object):
    """Manage names allocation. Safe for parallel usage."""

    class Error(Exception):
        def __init__(self, reservations, message):
            super(Exception, self).__init__("current reservations:\n{}".format(message, pformat(reservations)))
            self.reservations = reservations

    class NotReserved(Exception):
        def __init__(self, reservations, index):
            super(Registry.NotReserved, self).__init__(reservations, "Index {} is not reserved".format(index))
            self.index = index

    def __init__(self, os_access, path):
        self._os_access = os_access
        self._path = self._os_access.expand_path(path)
        self._lock = MoveLock(self._os_access, self._path.with_suffix('.lock'))
        if not self._os_access.dir_exists(self._path.parent):
            logger.warning("Create %r; OK only if a clean run", self._path.parent)
            self._os_access.mk_dir(self._path.parent)

    def __repr__(self):
        return '<Registry {}>'.format(self._path)

    def _read_reservations(self):
        try:
            contents = self._os_access.read_file(self._path)
        except FileNotFound:
            return []
        return load(contents)

    def reserve(self, alias):
        with self._lock:
            reservations = self._read_reservations()
            try:
                index = reservations.index(None)
            except ValueError:
                index = len(reservations)
                reservations.append(alias)
            else:
                reservations[index] = alias
            self._os_access.write_file(self._path, dump(reservations))
            logger.info("Index %.03d taken in %r.", index, self)
            return index

    def relinquish(self, index):
        with self._lock:
            reservations = self._read_reservations()
            try:
                alias = reservations[index]
            except IndexError:
                alias = None
            if alias is not None:
                reservations[index] = None
                self._os_access.write_file(self._path, dump(reservations))
                logger.info("Index %.03d freed in %r.", index, self)
            else:
                raise self.NotReserved(reservations, index)

    def for_each(self, procedure):
        with self._lock:
            reservations = self._read_reservations()
            for index, alias in enumerate(reservations):
                procedure(index, alias)