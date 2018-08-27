import pytest

from framework.installation.dpkg_installation import DpkgInstallation
from framework.installation.installation import OsNotSupported
from framework.installation.windows_installation import WindowsInstallation

# TODO: Integrate into configuration.yaml.
_vm_type_to_platform = {
    'linux': 'linux64',
    'windows': 'win64',
    }


def make_installation(os_access, customization):
    factories = [
        lambda: DpkgInstallation(os_access, customization.linux_subdir),
        lambda: WindowsInstallation(os_access, customization)]
    for factory in factories:
        try:
            return factory()
        except OsNotSupported:
            continue
    raise ValueError("No installation types exist for {!r}".format(os_access))
