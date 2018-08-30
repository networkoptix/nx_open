import logging
import re
from collections import namedtuple
from pprint import pformat

from pathlib2 import PurePosixPath, PureWindowsPath

_logger = logging.getLogger(__name__)


class Version(tuple):  # `tuple` gives `__hash__` and comparisons but requires `__new__`.
    def __new__(cls, version_str):
        return super(Version, cls).__new__(cls, map(int, version_str.split('.', 3)))

    def __init__(self, version_str):
        super(Version, self).__init__()
        self.major, self.minor, self.fix, self.build = self
        self.as_str = version_str
        self.short_as_str = '{}.{}'.format(self.major, self.minor)

    def __repr__(self):
        return 'Version({!r})'.format(self.as_str)

    def __str__(self):
        return self.as_str


class PackageNameParseError(Exception):
    pass


Customization = namedtuple('Customization', [
    'customization_name', 'installer_name', 'company_name',
    'linux_service_name', 'linux_subdir',
    'windows_service_name', 'windows_installation_subdir', 'windows_app_data_subdir', 'windows_registry_key',
    ])

known_customizations = {
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
    Customization(
        customization_name='digitalwatchdog',
        installer_name='dwspectrum',
        company_name='Digital Watchdog',
        linux_service_name='dwspectrum-mediaserver',
        linux_subdir=PurePosixPath('dwspectrum/mediaserver'),
        windows_service_name='dwspectrumMediaServer',
        windows_installation_subdir=PureWindowsPath(u'Digital Watchdog', u'DW Spectrum', u'MediaServer'),
        windows_app_data_subdir=PureWindowsPath(u'Digital Watchdog', u'Digital Watchdog Media Server'),
        windows_registry_key=u'HKEY_LOCAL_MACHINE\\SOFTWARE\\Digital Watchdog\\Digital Watchdog Media Server',
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
    for customization in known_customizations:
        if getattr(customization, field_name) == field_value:
            found.append(customization)
    if not found:
        raise UnknownCustomization(field_name, field_value)
    try:
        one, = found
    except ValueError:
        raise AmbiguousCustomization(field_name, field_value, found)
    return one


class InstallIdentity(object):
    """Identity for installer or installation"""

    @classmethod
    def from_build_info(cls, build_info):
        return cls(
            Version(build_info['version']),
            find_customization('customization_name', build_info['customization']),
            )

    def __init__(self, version, customization):
        self.version = version  # type: Version
        self.customization = customization  # type: Customization

    def __str__(self):
        return '{}:{}'.format(self.version, self.customization.customization_name)

    def __repr__(self):
        return '<InstallationIdentity {!s}>'.format(self)

    def __eq__(self, other):
        return (isinstance(other, InstallIdentity) and
                self.version == other.version and
                self.customization == other.customization)

    def __hash__(self):
        return hash((self.version, self.customization))


class Installer(object):
    ## Installer name regex.
    # See: https://networkoptix.atlassian.net/wiki/spaces/SD/pages/79462475/Installer+Filenames.
    _name_re = re.compile(
        r'''
            ^ (?P<name> \w+ )
            - (?P<component> \w+ )
            - (?P<version> \d+\.\d+\.\d+ (?: \.\d+ )? )
            - (?P<platform> \w+ )
            (?: - (?P<beta> beta ) )?
            (?: - (?P<cloud_group> \w+ ) )?
            (?P<extension> \.\w+ | \.tar(?: \.gz | \.xz | \.bz2) )
            $
        ''',
        re.VERBOSE)

    ## OS, its variant and architecture is encoded in one part of the name. This is the mapping.
    _platform_arch = {
        'linux64': (('linux', 'ubuntu', '14.04'), 'x64'),
        'linux86': (('linux', 'ubuntu', '14.04'), 'x86'),
        'win64': (('win', 'none', '7.0'), 'x64'),
        'win86': (('win', 'none', '7.0'), 'x86'),
        }

    def __init__(self, path):
        m = self._name_re.match(path.name)
        if m is None:
            raise PackageNameParseError("Installer name not understood: {}".format(path.name))
        _logger.debug("Parsed name: %s", pformat(m.groupdict()))
        self.extension = m.group('extension')
        try:
            self.customization = find_customization('installer_name', m.group('name'))
        except UnknownCustomization as e:
            raise PackageNameParseError("{}: {}".format(e, path.name))
        self.component = m.group('component')
        self.version = Version(m.group('version'))
        platform_tuple, self.arch = self._platform_arch[m.group('platform')]
        self.platform, self.platform_variant, self.platform_variant_version = platform_tuple
        self.is_beta = bool(m.group('beta'))
        self.cloud_group = m.group('cloud_group') or None
        self.identity = InstallIdentity(self.version, self.customization)
        self.path = path

    def __repr__(self):
        return 'Installer({!r})'.format(self.path)


class InstallerSet(object):
    def __init__(self, installers_dir):
        self.installers = []
        for path in installers_dir.glob('*'):
            try:
                installer = Installer(path)
            except PackageNameParseError as e:
                _logger.debug("File {}: {!s}".format(path, e))
                continue
            _logger.info("File {}: {!r}".format(path, installer))
            self.installers.append(installer)
        customizations = {installer.customization for installer in self.installers}
        try:
            self.customization, = customizations
        except ValueError:
            raise ValueError("Expected one, found: {!r}".format(customizations))
        versions = {installer.version for installer in self.installers}
        try:
            self.version, = versions
        except ValueError:
            raise ValueError("Expected one, found: {!r}".format(versions))
