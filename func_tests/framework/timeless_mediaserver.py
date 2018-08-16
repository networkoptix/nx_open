from contextlib import contextmanager

from framework.installation.mediaserver_factory import allocated_mediaserver


@contextmanager
def timeless_mediaserver(vm, mediaserver_installers, ca, artifacts_dir):
    """Mediaserver never exposed to internet depending on machine time"""
    with allocated_mediaserver(
            mediaserver_installers, artifacts_dir,
            ca, vm.alias, vm) as mediaserver:
        vm.os_access.networking.disable_internet()
        mediaserver.installation.update_mediaserver_conf({
            'ecInternetSyncTimePeriodSec': 3,
            'ecMaxInternetTimeSyncRetryPeriodSec': 3,
        })
        mediaserver.start()
        mediaserver.api.setup_local_system()
        yield mediaserver
