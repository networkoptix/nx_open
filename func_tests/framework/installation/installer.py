from collections import namedtuple


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
    'customization_name', 'installer_name',
    'company_name',
    'linux_service_name', 'linux_subdir',
    'windows_service_name', 'windows_installation_subdir', 'windows_app_data_subdir',
    ])

known_customizations = {
    Customization(
        'hanwha', 'wave',
        'Hanwha',
        'hanwha-mediaserver', 'hanwha/mediaserver',
        'hanwhaMediaServer', 'Hanwha\\Wisenet WAVE\\MediaServer', 'Hanwha\\Hanwha Media Server',
        ),
    Customization(
        'default', 'nxwitness',
        'Network Optix',
        'networkoptix-mediaserver', 'networkoptix/mediaserver',
        'defaultMediaServer', 'Network Optix\\Nx Witness\\MediaServer', 'Network Optix\\Network Optix Media Server',
        ),
    }


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
        try:
            customization, = (
                customization
                for customization in known_customizations
                if customization.installer_name == installer_name)
        except ValueError:
            raise PackageNameParseError("Customization with installer name {} is unknown".format(installer_name))
        self.customization = customization  # type: Customization
        self.path = path

    def __repr__(self):
        return 'Installer({!r})'.format(self.path)
