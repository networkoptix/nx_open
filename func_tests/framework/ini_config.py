from io import BytesIO

from framework.os_access.exceptions import DoesNotExist
from framework.os_access.posix_access import PosixAccess
from framework.os_access.windows_access import WindowsAccess

try:
    # noinspection PyCompatibility
    from ConfigParser import SafeConfigParser as ConfigParser, NoOptionError, DEFAULTSECT
except ImportError:
    # noinspection PyCompatibility
    from configparser import ConfigParser, NoOptionError, DEFAULTSECT


class NameNotFound(Exception):
    pass


class IniConfig(object):
    """See: https://networkoptix.atlassian.net/wiki/spaces/SD/pages/83895081."""

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
        self._parser = None  # type: ConfigParser
        self.reload()

    def __repr__(self):
        return '<IniConfig at {!r}>'.format(self._path)

    def reload(self):
        try:
            data = self._path.read_bytes()
        except DoesNotExist:
            self._parser = ConfigParser()
        else:
            buffer = BytesIO(data)
            self._parser = ConfigParser()
            self._parser.readfp(buffer)

    def _write(self):
        buffer = BytesIO()
        self._parser.write(buffer)
        data = buffer.getvalue()
        self._path.parent.mkdir(parents=True, exist_ok=True)
        self._path.write_bytes(data)

    def get(self, name):
        try:
            return self._parser.get(DEFAULTSECT, name)
        except NoOptionError:
            raise NameNotFound("No {} in {}".format(name, self._path))

    def set(self, name, value):
        self._parser.set(DEFAULTSECT, name, value)
        self._write()

    def delete(self, name):
        try:
            self._parser.remove_option(DEFAULTSECT, name)
        except NoOptionError:
            raise NameNotFound("No {} in {}".format(name, self._path))
        self._write()
