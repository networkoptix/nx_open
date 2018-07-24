import pytest

from framework.installation.dpkg_installation import DpkgInstallation
from framework.installation.windows_installation import WindowsInstallation

# TODO: Integrate into configuration.yaml.
_vm_type_to_platform = {
    'linux': 'linux64',
    'windows': 'win64',
    }


def installer_by_vm_type(mediaserver_installers, vm_type):
    platform = _vm_type_to_platform[vm_type]
    try:
        return mediaserver_installers[platform]
    except KeyError:
        pytest.skip("Mediaserver installer for {} ({}) is not provided for tests".format(vm_type, platform))
        assert False, "Skip should raise exception"


def make_installation(mediaserver_installers, vm_type, os_access):
    installer = installer_by_vm_type(mediaserver_installers, vm_type)
    if vm_type == 'linux':
        return DpkgInstallation(os_access, installer.customization.linux_subdir)
    if vm_type == 'windows':
        return WindowsInstallation(os_access, installer.identity)
    raise ValueError("Unknown VM type {}".format(vm_type))
