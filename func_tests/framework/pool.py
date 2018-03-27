import logging

logger = logging.getLogger(__name__)


class Pool(object):
    """Allocate multiple items, dispose all at once with .close()"""

    def __init__(self, factory):
        self._factory = factory
        self._cache = {}

    def get(self, alias):
        try:
            cached_item = self._cache[alias]
        except KeyError:
            new_item = self._factory.allocate(alias)
            self._cache[alias] = new_item
            logger.debug("Allocated: %r.", new_item)
            return new_item
        else:
            logger.debug("Reused: %r.", cached_item)
            return cached_item

    def close(self):
        for item in self._cache.values():
            self._factory.release(item)
            logger.debug("Disposed: %r.", item)
