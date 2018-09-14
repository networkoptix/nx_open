import logging

from netaddr import IPNetwork

from framework.installation.mediaserver import Mediaserver
from .context_logger import context_logger

_logger = logging.getLogger(__name__)
_merge_logger = logging.getLogger('framework.mediaserver_api.merge')


@context_logger(_merge_logger, 'framework.http_api')
@context_logger(_merge_logger, 'framework.mediaserver_api')
def merge_systems(
        local,  # type: Mediaserver  # Request will be sent to this.
        remote,  # type: Mediaserver
        take_remote_settings=False,
        accessible_ip_net=IPNetwork('10.254.0.0/16'),
        ):
    remote_interfaces = remote.api.interfaces()
    try:
        remote_address = next(interface.ip for interface in remote_interfaces if interface.ip in accessible_ip_net)
    except StopIteration:
        raise RuntimeError('Unable to merge systems: none of interfaces %r of %r are in network %r' % (
            remote_interfaces, remote, accessible_ip_net))
    local.api.merge(remote.api, remote_address, remote.port, take_remote_settings=take_remote_settings)


def setup_system(mediaservers, scheme):
    # Local is one to which request is sent.
    # Remote's URL is sent included in request to local.
    for merge_parameters in scheme:
        local_mediaserver = mediaservers[merge_parameters['local']]
        remote_mediaserver = mediaservers[merge_parameters['remote']]
        merge_kwargs = {}
        if merge_parameters is not None:
            try:
                merge_kwargs['take_remote_settings'] = merge_parameters['settings'] == 'remote'
            except KeyError:
                pass
            try:
                remote_network = IPNetwork(merge_parameters['network'])
            except KeyError:
                pass
            else:
                merge_kwargs['accessible_ip_net'] = remote_network
        merge_systems(local_mediaserver, remote_mediaserver, **merge_kwargs)
