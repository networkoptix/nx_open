from contextlib import contextmanager


@contextmanager
def timeless_mediaserver(mediaserver_allocation, vm):
    """Mediaserver never exposed to internet depending on machine time"""
    with mediaserver_allocation(vm) as mediaserver:
        vm.os_access.networking.disable_internet()
        mediaserver.installation.update_mediaserver_conf({
            'ecInternetSyncTimePeriodSec': 3,
            'ecMaxInternetTimeSyncRetryPeriodSec': 3,
        })
        mediaserver.start()
        mediaserver.api.setup_local_system()
        yield mediaserver
