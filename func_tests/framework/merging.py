import logging

from netaddr import IPNetwork

from framework.installation.mediaserver import Mediaserver
from .switched_logging import with_logger

_logger = logging.getLogger(__name__)
_merge_logger = logging.getLogger('framework.mediaserver_api.merge')


@with_logger(_merge_logger, 'framework.http_api')
@with_logger(_merge_logger, 'framework.mediaserver_api')
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


def setup_system(allocate_mediaserver, scheme):
    allocated_mediaservers = {}

    def get_mediaserver(alias):
        """Get running server with local system set up; save it in allocated_mediaservers."""
        try:
            return allocated_mediaservers[alias]
        except KeyError:
            allocated_mediaservers[alias] = new_mediaserver = allocate_mediaserver(alias)
            new_mediaserver.start()
            new_mediaserver.api.setup_local_system()
            return new_mediaserver

    # Local is one to which request is sent.
    # Remote's URL is sent included in request to local.
    for remote_mediaserver_alias, local_mediaserver_aliases in scheme.items():
        remote_mediaserver = get_mediaserver(remote_mediaserver_alias)
        for local_mediaserver_alias, merge_parameters in (local_mediaserver_aliases or {}).items():
            local_mediaserver = get_mediaserver(local_mediaserver_alias)
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
    return allocated_mediaservers
