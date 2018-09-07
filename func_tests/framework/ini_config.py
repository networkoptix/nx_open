import logging
from collections import OrderedDict

from typing import MutableMapping

from .context_logger import ContextAdapter
from framework.os_access.exceptions import DoesNotExist

_logger = logging.getLogger(__name__)


class NameNotFound(Exception):
    pass


class IniConfig(object):
    """See: https://networkoptix.atlassian.net/wiki/spaces/SD/pages/83895081."""

    _dict = OrderedDict

    def __init__(self, path):
        self._path = path
        self._cached_settings = None  # type: MutableMapping
        self._logger = ContextAdapter(_logger, self)
        self.reload()

    def __str__(self):
        return 'IniConfig at {!r}'.format(self._path)

    def __repr__(self):
        return '<{}>'.format(self)

    def reload(self):
        try:
            data = self._path.read_text()
        except DoesNotExist:
            self._logger.info('Missing.')
            self._cached_settings = self._dict()
        else:
            self._logger.info('Loaded:\n%s', data)
            # Config here is parsed by hand, because IniConfig from C++ doesn't
            # understand sections, which are always created by ConfigParser.
            self._cached_settings = self._dict(
                line.split(u'=', 1) for line in data.splitlines())

    def _write(self):
        data = u'\n'.join(
            u'{}={}'.format(name, value)
            for name, value in self._cached_settings.items())
        self._logger.info('Save:\n%s', data)
        self._path.parent.mkdir(exist_ok=True, parents=True)
        self._path.write_text(data)

    def get(self, name):
        try:
            return self._cached_settings[name]
        except KeyError:
            raise NameNotFound('{!r}: no name {!r}'.format(self, name))

    def set(self, name, value):
        self._cached_settings[name] = value
        self._write()

    def delete(self, name):
        del self._cached_settings[name]
        self._write()
