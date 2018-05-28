import pytest

from framework.installation.deb_installation import DebInstallation
from framework.installation.windows_installation import WindowsInstallation


def make_installation(mediaserver_installers, vm_type, os_access):
    if vm_type == 'linux':
        if 'linux64' not in mediaserver_installers:
            pytest.skip("Mediaserver installer %r is not provided for tests" % vm_type)
        local_deb = mediaserver_installers['linux64']
        linux_installation = DebInstallation(os_access, local_deb)
        return linux_installation
    if vm_type == 'windows':
        if 'win64' not in mediaserver_installers:
            pytest.skip("Mediaserver installer %r is not provided for tests" % vm_type)
        local_windows_installer = mediaserver_installers['win64']
        windows_installation = WindowsInstallation(os_access, local_windows_installer)
        return windows_installation
    raise ValueError("Unknown VM type {}".format(vm_type))
