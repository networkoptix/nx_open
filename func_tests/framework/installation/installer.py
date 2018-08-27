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
    """Information that can be extracted from package name."""
    platform_extensions = [
        ('linux64', '.deb'),
        ('linux86', '.deb'),
        ('win64', '.exe'), ('win64', '.msi'),
        ('win86', '.exe'), ('win86', '.msi'),
        ('mac', '.dmg'),
        ('bpi', '.zip'), ('bpi', '.tar.gz'),
        ('bananapi', '.zip'), ('bananapi', '.tar.gz'),
        ('tx1', '.zip'), ('tx1', '.tar.gz'),
        ('edge1', '.zip'), ('edge1', '.tar.gz'),
        ]

    def __init__(self, path):
        # If there were no `.tar.gz`, path.suffix would suffice.
        for possible_platform, extension in self.__class__.platform_extensions:
            if path.name.endswith(extension):
                stem = path.name[:-len(extension)]
                beta_test_suffix = '-beta-test'
                try:
                    if stem.endswith(beta_test_suffix):
                        installer_name, product, version_str, platform = stem[:-len(beta_test_suffix)].split('-', 3)
                    else:
                        installer_name, product, version_str, platform = stem.split('-', 3)
                except ValueError:
                    raise PackageNameParseError("Format is not name-product-version-platform: {}".format(path.name))
                if product != 'server':
                    raise PackageNameParseError("This is a {} but only server is supported".format(product))
                break
        else:
            raise PackageNameParseError("Unknown extension or platform: {}".format(path.name))
        self.platform = platform
        self.version = Version(version_str)
        self.customization = find_customization('installer_name', installer_name)
        self.identity = InstallIdentity(self.version, self.customization)
        self.path = path

    def __repr__(self):
        return 'Installer({!r})'.format(self.path)
