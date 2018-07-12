from contextlib import contextmanager

from framework.installation.make_installation import installer_by_vm_type, make_installation
from framework.installation.mediaserver_factory import (
    cleanup_mediaserver,
    collect_artifacts_from_mediaserver,
    make_dirty_mediaserver,
    )
from framework.merging import setup_local_system


@contextmanager
def timeless_mediaserver(vm, mediaserver_installers, ca, artifacts_dir):
    """Mediaserver never exposed to internet depending on machine time"""
    installer = installer_by_vm_type(mediaserver_installers, vm.type)
    installation = make_installation(mediaserver_installers, vm.type, vm.os_access)
    mediaserver = make_dirty_mediaserver(vm.alias, installation, installer)
    mediaserver.stop(already_stopped_ok=True)
    vm.os_access.networking.disable_internet()
    mediaserver.installation.cleanup(ca.generate_key_and_cert())
    mediaserver.installation.update_mediaserver_conf({
        'ecInternetSyncTimePeriodSec': 3,
        'ecMaxInternetTimeSyncRetryPeriodSec': 3,
        })
    mediaserver.start()
    setup_local_system(mediaserver, {})
    try:
        yield mediaserver
    finally:
        collect_artifacts_from_mediaserver(mediaserver, artifacts_dir)
