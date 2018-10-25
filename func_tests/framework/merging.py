import logging

from netaddr import IPNetwork
from typing import Mapping
from typing import Sequence

from framework.installation.mediaserver import Mediaserver
from framework.mediaserver_api import DEFAULT_TAKE_REMOTE_SETTINGS
from .context_logger import context_logger

DEFAULT_ACCESSIBLE_IP_NET = IPNetwork('10.254.0.0/16')

_logger = logging.getLogger(__name__)
_merge_logger = logging.getLogger('framework.mediaserver_api.merge')


@context_logger(_merge_logger, 'framework.http_api')
@context_logger(_merge_logger, 'framework.mediaserver_api')
def merge_systems(
        local,  # type: Mediaserver  # Request will be sent to this.
        remote,  # type: Mediaserver
        take_remote_settings=DEFAULT_TAKE_REMOTE_SETTINGS,
        accessible_ip_net=DEFAULT_ACCESSIBLE_IP_NET,
        timeout_sec=30,
        ):
    remote_interfaces = remote.api.interfaces()
    try:
        remote_address = next(interface.ip for interface in remote_interfaces if interface.ip in accessible_ip_net)
    except StopIteration:
        raise RuntimeError('Unable to merge systems: none of interfaces %r of %r are in network %r' % (
            remote_interfaces, remote, accessible_ip_net))
    local.api.merge(remote.api, remote_address, remote.port, take_remote_settings, timeout_sec)


def setup_system(mediaservers, scheme):
    # type: (Mapping[str, Mediaserver], Sequence[Mapping[str, ...]]) -> None
    """Request is sent to "local". It's asked to merge with "remote" and provided base URL of
    API of "remote".
    """
    for merger in scheme:
        merge_systems(
            mediaservers[merger['local']],
            mediaservers[merger['remote']],
            take_remote_settings=merger.get('take_remote_settings', DEFAULT_TAKE_REMOTE_SETTINGS),
            accessible_ip_net=IPNetwork(merger.get('network', DEFAULT_ACCESSIBLE_IP_NET)))
