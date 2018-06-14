from collections import namedtuple
from pprint import pformat

from pathlib2 import PurePosixPath, PureWindowsPath


class Version(tuple):  # `tuple` gives `__hash__` and comparisons but requires `__new__`.
    def __new__(cls, version_str):
        return super(Version, cls).__new__(cls, map(int, version_str.split('.', 3)))

    def __init__(self, version_str):
        super(Version, self).__init__()
        self.major, self.minor, self.fix, self.build = self
        self.as_str = version_str

    def __repr__(self):
        return 'Version({!r})'.format(self.as_str)


class PackageNameParseError(Exception):
    pass


Customization = namedtuple('Customization', [
    'customization_name', 'installer_name', 'company_name',
    'linux_service_name', 'linux_subdir',
    'windows_service_name', 'windows_installation_subdir', 'windows_app_data_subdir', 'windows_registry_key',
    ])

_known_customizations = {
    Customization(
        customization_name='hanwha',
        installer_name='wave',
        company_name='Hanwha',
        linux_service_name='hanwha-mediaserver',
        linux_subdir=PurePosixPath('hanwha', 'mediaserver'),
        windows_service_name='hanwhaMediaServer',
        windows_installation_subdir=PureWindowsPath(u'Hanwha', u'Wisenet WAVE', u'MediaServer'),
        windows_app_data_subdir=PureWindowsPath(u'Hanwha', u'Hanwha Media Server'),
        windows_registry_key=u'HKEY_LOCAL_MACHINE\\SOFTWARE\\Hanwha\\Hanwha Media Server',
        ),
    Customization(
        customization_name='default',
        installer_name='nxwitness',
        company_name='Network Optix',
        linux_service_name='networkoptix-mediaserver',
        linux_subdir=PurePosixPath('networkoptix/mediaserver'),
        windows_service_name='defaultMediaServer',
        windows_installation_subdir=PureWindowsPath(u'Network Optix', u'Nx Witness', u'MediaServer'),
        windows_app_data_subdir=PureWindowsPath(u'Network Optix', u'Network Optix Media Server'),
        windows_registry_key=u'HKEY_LOCAL_MACHINE\\SOFTWARE\\Network Optix\\Network Optix Media Server',
        ),
    Customization(
        customization_name='metavms',
        installer_name='metavms',
        company_name='Network Optix',
        linux_service_name='networkoptix-mediaserver',
        linux_subdir=PurePosixPath('networkoptix/mediaserver'),
        windows_service_name='metavmsMediaServer',
        windows_installation_subdir=PureWindowsPath(u'Network Optix', u'Nx MetaVMS', u'MediaServer'),
        windows_app_data_subdir=PureWindowsPath(u'Network Optix', u'Network Optix Media Server'),
        windows_registry_key=u'HKEY_LOCAL_MACHINE\\SOFTWARE\\Network Optix\\Network Optix Media Server',
        ),
    }


class UnknownCustomization(EnvironmentError):
    def __init__(self, field_name, field_value):
        super(UnknownCustomization, self).__init__(
            "No customization found with {!s}={!r}".format(
                field_name, field_value))
        self.field_name = field_name
        self.field_value = field_value


class AmbiguousCustomization(EnvironmentError):
    def __init__(self, field_name, field_value, customizations):
        super(AmbiguousCustomization, self).__init__(
            "Multiple customizations found with {!s}={!r}:\n{}".format(
                field_name, field_value, pformat(customizations)))
        self.field_name = field_name
        self.field_value = field_value
        self.customizations = customizations


def find_customization(field_name, field_value):
    found = []
    for customization in _known_customizations:
        if getattr(customization, field_name) == field_value:
            found.append(customization)
    if not found:
        raise UnknownCustomization(field_name, field_value)
    try:
        one, = found
    except ValueError:
        raise AmbiguousCustomization(field_name, field_value, found)
    return one


class Installer(object):
    """Information that can be extracted from package name."""
    _extensions = {'linux64': 'deb', 'win64': 'exe'}

    def __init__(self, path):
        try:
            stem, self.extension = path.name.rsplit('.', 1)
            installer_name, self.product, version_str, self.platform, self.maturity = stem.split('-', 4)
        except (TypeError, ValueError):
            raise PackageNameParseError("Bad format {}".format(path.name))
        try:
            platform_extension = self._extensions[self.platform]
        except KeyError:
            raise PackageNameParseError("Unknown platform {}".format(self.platform))
        if platform_extension != self.extension:
            raise PackageNameParseError("Extension of {} should be {}".format(path.name, platform_extension))
        self.version = Version(version_str)
        self.customization = find_customization('installer_name', installer_name)
        self.path = path

    def __repr__(self):
        return 'Installer({!r})'.format(self.path)
