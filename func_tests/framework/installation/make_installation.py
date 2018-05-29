import pytest

from framework.installation.deb_installation import DebInstallation
from framework.installation.windows_installation import WindowsInstallation

# TODO: Integrate into configuration.yaml.
_vm_type_to_platform = {
    'linux': 'linux64',
    'windows': 'win64',
    }


def make_installation(mediaserver_installers, vm_type, os_access):
    platform = _vm_type_to_platform[vm_type]
    try:
        installer = mediaserver_installers[platform]
    except KeyError:
        pytest.skip("Mediaserver installer for {} ({}) is not provided for tests".format(vm_type, platform))
        assert False, "Skip should raise exception"
    if vm_type == 'linux':
        return DebInstallation(os_access, installer)
    if vm_type == 'windows':
        return WindowsInstallation(os_access, installer)
    raise ValueError("Unknown VM type {}".format(vm_type))
