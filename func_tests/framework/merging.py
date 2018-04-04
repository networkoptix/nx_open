import logging

from netaddr import IPAddress, IPNetwork

from framework.api_shortcuts import get_local_system_id
from framework.rest_api import DEFAULT_API_PASSWORD, DEFAULT_API_USER, RestApiError
from framework.utils import wait_until

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


class MergeChecksFailed(MergeError):
    def __init__(self, local, remote, message):
        super(MergeChecksFailed, self).__init__(local, remote, "response was OK but checks failed: {}".format(message))
        self.local = local
        self.remote = remote


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
    if not wait_until(
            servant.api.credentials_work,
            name="until {} accepts new credentials from {}".format(servant, master),
            timeout_sec=30):
        raise MergeChecksFailed(local, remote, "new credentials don't work")
    if not wait_until(
            lambda: get_local_system_id(servant.api) == master_system_id,
            name="until {} responds with system id {}".format(servant, master_system_id),
            timeout_sec=10):
        raise MergeChecksFailed(local, remote, "local system ids don't match")


def setup_local_system(server, system_settings):
    _logger.info('Setup local system on %s.', server)
    response = server.api.post('api/setupLocalSystem', {
        'password': DEFAULT_API_PASSWORD,
        'systemName': server.name,
        'systemSettings': system_settings,
        })
    assert system_settings == {key: response['settings'][key] for key in system_settings.keys()}
    return response['settings']


def setup_cloud_system(server, cloud_account, system_settings):
    _logger.info('Setting up server as local system %s:', server)
    bind_info = cloud_account.bind_system(server.name)
    request = {
        'systemName': server.name,
        'cloudAuthKey': bind_info.auth_key,
        'cloudSystemID': bind_info.system_id,
        'cloudAccountName': cloud_account.api.user,
        'systemSettings': system_settings, }
    response = server.api.post('api/setupCloudSystem', request, timeout=300)
    assert system_settings == {key: response['settings'][key] for key in system_settings.keys()}
    # assert cloud_account.api.user == response['settings']['cloudAccountName']
    server.api = server.api.with_credentials(cloud_account.api.user, cloud_account.password)
    assert server.api.credentials_work()
    return response['settings']


def detach_from_cloud(server, password):
    server.api.post('api/detachFromCloud', {'password': password})
    server.api = server.api.with_credentials(DEFAULT_API_USER, password)


def setup_system(servers, scheme):
    # if config.setup_cloud_account:
    #     server.setup_cloud_system(config.setup_cloud_account, **config.setup_settings)
    #     if not config.leave_initial_cloud_host:
    #         server.api = server.api.with_credentials(DEFAULT_API_USER, DEFAULT_API_PASSWORD)  # TODO: Needed?
    # else:
    #     setup_local_system(server, dict((**config.setup_settings)))
    allocated_servers = {}
    for remote_alias, local_aliases in scheme.items():
        allocated_servers[remote_alias] = servers.get(remote_alias)  # Create server but don't merge.
        allocated_servers[remote_alias].start(already_started_ok=True)
        for local_alias, merge_parameters in (local_aliases or {}).items():
            allocated_servers[local_alias] = servers.get(local_alias)
            allocated_servers[local_alias].start(already_started_ok=True)
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
            merge_systems(allocated_servers[local_alias], allocated_servers[remote_alias], **merge_kwargs)
    return allocated_servers
