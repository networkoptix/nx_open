import logging

from netaddr import IPAddress, IPNetwork

from framework.mediaserver_api import MediaserverApiError
from framework.waiting import wait_for_true

_logger = logging.getLogger(__name__)


class MergeError(Exception):
    def __init__(self, local, remote, message):
        super(MergeError, self).__init__('Request {} to merge with {}: {}'.format(local, remote, message))
        self.local = local
        self.remote = remote


class AlreadyMerged(MergeError):
    def __init__(self, local, remote, common_system_id):
        super(AlreadyMerged, self).__init__(local, remote, "already in one local system {}".format(common_system_id))
        self.system_id = common_system_id


class NoAddressesError(Exception):
    def __init__(self, mediaserver, available_ips):
        super(NoAddressesError, self).__init__(
            "For mediaserver {} there is no IPs: {}".format(mediaserver, available_ips))
        self.mediaserver = mediaserver
        self.available_ips = available_ips


class NoAvailableAddressesError(Exception):
    def __init__(self, mediaserver, ip_net, available_ips):
        super(NoAvailableAddressesError, self).__init__(
            "For mediaserver {} there is no available IPs from {}: {}".format(mediaserver, ip_net, available_ips))
        self.mediaserver = mediaserver
        self.ip_net = ip_net
        self.available_ips = available_ips


class ExplicitMergeError(MergeError):
    def __init__(self, local, remote, error, error_string):
        super(ExplicitMergeError, self).__init__(local, remote, "{:d} {}".format(error, error_string))
        self.error = error
        self.error_string = error_string


class IncompatibleServersMerge(ExplicitMergeError):
    pass


class MergeUnauthorized(ExplicitMergeError):
    pass


def find_accessible_mediaserver_address(mediaserver, accessible_ip_net=IPNetwork('10.254.0.0/16')):
    interface_list = mediaserver.api.generic.get('api/iflist')
    ip_set = {IPAddress(interface['ipAddr']) for interface in interface_list}
    accessible_ip_set = {ip for ip in ip_set if ip in accessible_ip_net}
    try:
        return next(iter(accessible_ip_set))
    except StopIteration:
        raise NoAvailableAddressesError(mediaserver, accessible_ip_net, ip_set)


def find_any_mediaserver_address(mediaserver):
    interface_list = mediaserver.api.generic.get('api/iflist')
    ip_set = {IPAddress(interface['ipAddr']) for interface in interface_list}
    try:
        return next(iter(ip_set))
    except StopIteration:
        raise NoAddressesError(mediaserver, ip_set)


def merge_systems(
        local,
        remote,
        take_remote_settings=False,
        remote_address=None,
        ):
    # When many servers are merged, there is server visible from others.
    # This server is passed as remote. That's why it's higher in loggers hierarchy.
    merge_logger = _logger.getChild('merge').getChild(remote.name).getChild(local.name)
    merge_logger.info("Request %r to merge %r (takeRemoteSettings: %s).", local, remote, take_remote_settings)
    master, servant = (remote, local) if take_remote_settings else (local, remote)
    master_system_id = master.api.get_local_system_id()
    merge_logger.debug("Settings from %r, system id %s.", master, master_system_id)
    servant_system_id = servant.api.get_local_system_id()
    merge_logger.debug("Other system id %s.", servant_system_id)
    if servant_system_id == master_system_id:
        raise AlreadyMerged(local, remote, master_system_id)
    if remote_address is None:
        remote_address = find_accessible_mediaserver_address(remote)
    merge_logger.debug("Access %r by %s.", remote, remote_address)
    try:
        local.api.generic.post('api/mergeSystems', {
            'url': 'http://{}:{}/'.format(remote_address, remote.port),
            'getKey': remote.api.auth_key('GET'),
            'postKey': remote.api.auth_key('POST'),
            'takeRemoteSettings': take_remote_settings,
            'mergeOneServer': False,
            })
    except MediaserverApiError as e:
        if e.error_string == 'INCOMPATIBLE':
            raise IncompatibleServersMerge(local, remote, e.error, e.error_string)
        if e.error_string == 'UNAUTHORIZED':
            raise MergeUnauthorized(local, remote, e.error, e.error_string)
        raise ExplicitMergeError(local, remote, e.error, e.error_string)
    servant.api.generic.http.set_credentials(master.api.generic.http.user, master.api.generic.http.password)
    wait_for_true(servant.api.credentials_work, timeout_sec=30)
    wait_for_true(
        lambda: servant.api.get_local_system_id() == master_system_id,
        "{} responds with system id {}".format(servant, master_system_id),
        timeout_sec=10)


def setup_system(mediaservers, scheme):
    allocated_mediaservers = {}

    def get_mediaserver(alias):
        """Get running server with local system set up; save it in allocated_mediaservers."""
        try:
            return allocated_mediaservers[alias]
        except KeyError:
            allocated_mediaservers[alias] = new_mediaserver = mediaservers.get(alias)
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
