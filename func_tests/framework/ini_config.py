import logging
from collections import OrderedDict

from typing import MutableMapping

from framework.os_access.exceptions import DoesNotExist
from framework.os_access.posix_access import PosixAccess
from framework.os_access.windows_access import WindowsAccess

_logger = logging.getLogger(__name__)


class NameNotFound(Exception):
    pass


class IniConfig(object):
    """See: https://networkoptix.atlassian.net/wiki/spaces/SD/pages/83895081."""

    _dict = OrderedDict

    def __init__(self, os_access, name):
        env_vars = os_access.env_vars()
        try:
            ini_dir_raw = env_vars['NX_INI_DIR']
        except KeyError:
            if isinstance(os_access, WindowsAccess):
                ini_dir_raw = env_vars['LOCALAPPDATA'] + '\\nx_ini'
            elif isinstance(os_access, PosixAccess):
                ini_dir_raw = os_access.Path.home() / '.config' / 'nx_ini'
            else:
                raise ValueError("Unknown type of os_access: {}".format(os_access.__class__))
        self._path = os_access.Path(ini_dir_raw, name + '.ini')
        self._cached_settings = None  # type: MutableMapping
        self.reload()

    def __repr__(self):
        return '<IniConfig at {!r}>'.format(self._path)

    def reload(self):
        try:
            data = self._path.read_text()
        except DoesNotExist:
            self._cached_settings = self._dict()
        else:
            # Config here is parsed by hand, because IniConfig from C++ doesn't
            # understand sections, which are always created by ConfigParser.
            self._cached_settings = self._dict(
                line.split(u'=', 1) for line in data.splitlines())

    def _write(self):
        self._path.parent.mkdir(exist_ok=True, parents=True)
        data = u'\n'.join(
            u'{}={}'.format(name, value)
            for name, value in self._cached_settings.items())
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
