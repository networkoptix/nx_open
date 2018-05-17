from framework.installation.deb_installation import DebInstallation
from framework.installation.mediaserver_deb import MediaserverDeb
from framework.installation.windows_installation import WindowsInstallation


def make_installation(mediaserver_packages, vm_type, os_access):
    if vm_type == 'linux':
        local_deb_path = mediaserver_packages['deb']
        linux_installation = DebInstallation(os_access, local_deb_path)
        return linux_installation
    raise ValueError("Unknown VM type {}".format(vm_type))
