import logging
from uuid import UUID

from netaddr import IPAddress, IPNetwork

from framework.api_shortcuts import get_local_system_id
from framework.rest_api import DEFAULT_API_PASSWORD, DEFAULT_API_USER, INITIAL_API_PASSWORD, RestApiError
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


class MergeAddressesError(MergeError):
    def __init__(self, local, remote, ip_net, available_ips):
        super(MergeAddressesError, self).__init__(local, remote, "no IPs from {}: {}".format(ip_net, available_ips))
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


def merge_systems(local, remote, accessible_ip_net=IPNetwork('10.254.0.0/16'), take_remote_settings=False):
    # When many servers are merged, there is server visible from others.
    # This server is passed as remote. That's why it's higher in loggers hierarchy.
    merge_logger = _logger.getChild('merge').getChild(remote.name).getChild(local.name)
    merge_logger.info("Request %r to merge %r (takeRemoteSettings: %s).", local, remote, take_remote_settings)
    master, servant = (remote, local) if take_remote_settings else (local, remote)
    master_system_id = get_local_system_id(master.api)
    merge_logger.debug("Settings from %r, system id %s.", master, master_system_id)
    servant_system_id = get_local_system_id(servant.api)
    merge_logger.debug("Other system id %s.", servant_system_id)
    if servant_system_id == master_system_id:
        raise AlreadyMerged(local, remote, master_system_id)
    remote_interfaces = remote.api.get('api/iflist')
    available_remote_ips = {IPAddress(interface['ipAddr']) for interface in remote_interfaces}
    accessible_remote_ips = {ip for ip in available_remote_ips if ip in accessible_ip_net}
    try:
        any_accessible_remote_ip = next(iter(accessible_remote_ips))
    except StopIteration:
        raise MergeAddressesError(local, remote, accessible_ip_net, available_remote_ips)
    merge_logger.debug("Access %r by %s.", remote, any_accessible_remote_ip)
    try:
        local.api.post('api/mergeSystems', {
            'url': remote.api.with_hostname_and_port(any_accessible_remote_ip, remote.port).url(''),
            'getKey': remote.api.auth_key('GET'),
            'postKey': remote.api.auth_key('POST'),
            'takeRemoteSettings': take_remote_settings,
            'mergeOneServer': False,
            })
    except RestApiError as e:
        if e.error_string == 'INCOMPATIBLE':
            raise IncompatibleServersMerge(local, remote, e.error, e.error_string)
        if e.error_string == 'UNAUTHORIZED':
            raise MergeUnauthorized(local, remote, e.error, e.error_string)
        raise ExplicitMergeError(local, remote, e.error, e.error_string)
    servant.api = servant.api.with_credentials(master.api.user, master.api.password)
    wait_for_true(servant.api.credentials_work, timeout_sec=30)
    wait_for_true(
        lambda: get_local_system_id(servant.api) == master_system_id,
        "{} responds with system id {}".format(servant, master_system_id),
        timeout_sec=10)


def local_system_is_set_up(mediaserver):
    local_system_id = get_local_system_id(mediaserver.api)
    return local_system_id != UUID(int=0)


def setup_local_system(mediaserver, system_settings):
    _logger.info('Setup local system on %s.', mediaserver)
    response = mediaserver.api.post('api/setupLocalSystem', {
        'password': DEFAULT_API_PASSWORD,
        'systemName': mediaserver.name,
        'systemSettings': system_settings,
        })
    assert system_settings == {key: response['settings'][key] for key in system_settings.keys()}
    mediaserver.api = mediaserver.api.with_credentials(mediaserver.api.user, DEFAULT_API_PASSWORD)
    wait_for_true(lambda: local_system_is_set_up(mediaserver), "local system is set up")
    return response['settings']


def setup_cloud_system(mediaserver, cloud_account, system_settings):
    _logger.info('Setting up server as local system %s:', mediaserver)
    bind_info = cloud_account.bind_system(mediaserver.name)
    request = {
        'systemName': mediaserver.name,
        'cloudAuthKey': bind_info.auth_key,
        'cloudSystemID': bind_info.system_id,
        'cloudAccountName': cloud_account.api.user,
        'systemSettings': system_settings, }
    response = mediaserver.api.post('api/setupCloudSystem', request, timeout=300)
    assert system_settings == {key: response['settings'][key] for key in system_settings.keys()}
    # assert cloud_account.api.user == response['settings']['cloudAccountName']
    mediaserver.api = mediaserver.api.with_credentials(cloud_account.api.user, cloud_account.password)
    assert mediaserver.api.credentials_work()
    return response['settings']


def detach_from_cloud(server, password):
    server.api.post('api/detachFromCloud', {'password': password})
    server.api = server.api.with_credentials(DEFAULT_API_USER, password)


def setup_system(mediaservers, scheme):
    allocated_mediaservers = {}

    def get_mediaserver(alias):
        """Get running server with local system set up; save it in allocated_mediaservers."""
        try:
            return allocated_mediaservers[alias]
        except KeyError:
            allocated_mediaservers[alias] = new_mediaserver = mediaservers.get(alias)
            new_mediaserver.start()
            setup_local_system(new_mediaserver, {})
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
