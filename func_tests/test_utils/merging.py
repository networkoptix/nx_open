import logging

from netaddr import IPAddress, IPNetwork

from test_utils.api_shortcuts import get_local_system_id
from test_utils.rest_api import RestApiError
from test_utils.utils import wait_until

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


class MergeChecksFailed(MergeError):
    def __init__(self, local, remote, message):
        super(MergeChecksFailed, self).__init__(local, remote, "response was OK but checks failed: {}".format(message))
        self.local = local
        self.remote = remote


def _find_accessible_ips(api, ip_net):
    interfaces = api.get('api/iflist')
    available_ips = {IPAddress(interface['ipAddr']) for interface in interfaces}
    ips = {ip for ip in available_ips if ip in ip_net}
    return ips


def merge_systems(local, remote, accessible_ip_net=IPNetwork('10.254.0.0/17'), take_remote_settings=False):
    # When many servers are merged, there is server visible from others.
    # This server is passed as remote. That's why it's higher in loggers hierarchy.
    merge_logger = _logger.getChild('merge').getChild(remote.name).getChild(local.name)
    merge_logger.info("Request %r to merge %r (takeRemoteSettings: %s).", local, remote, take_remote_settings)
    master, servant = (remote, local) if take_remote_settings else (local, remote)
    master_system_id = get_local_system_id(master.rest_api)
    merge_logger.debug("Settings from %r, system id %s.", master, master_system_id)
    servant_system_id = get_local_system_id(servant.rest_api)
    merge_logger.debug("Other system id %s.", servant_system_id)
    if servant_system_id == master_system_id:
        raise AlreadyMerged(local, remote, master_system_id)
    accessible_remote_ips = _find_accessible_ips(remote.rest_api, accessible_ip_net)
    try:
        any_accessible_remote_ip = next(iter(accessible_remote_ips))
    except StopIteration:
        raise MergeAddressesError(local, remote, accessible_ip_net, accessible_remote_ips)
    merge_logger.debug("Access %r by %s.", remote, any_accessible_remote_ip)
    try:
        local.rest_api.post('api/mergeSystems', {
            'url': remote.rest_api.with_hostname_and_port(any_accessible_remote_ip, remote.port).url(''),
            'getKey': remote.rest_api.auth_key('GET'),
            'postKey': remote.rest_api.auth_key('POST'),
            'takeRemoteSettings': take_remote_settings,
            'mergeOneServer': False,
            })
    except RestApiError as e:
        raise ExplicitMergeError(local, remote, e.error, e.error_string)
    servant.rest_api = servant.rest_api.with_credentials(master.rest_api.user, master.rest_api.password)
    if not wait_until(
            servant.rest_api.credentials_work,
            name="until {} accepts new credentials from {}".format(servant, master),
            timeout_sec=10):
        raise MergeChecksFailed(local, remote, "new credentials don't work")
    if not wait_until(
            lambda: get_local_system_id(servant.rest_api) == master_system_id,
            name="until {} responds with system id {}".format(servant, master_system_id),
            timeout_sec=10):
        raise MergeChecksFailed(local, remote, "local system ids don't match")


def merge_system(server_factory, scheme):
    servers = {}
    for remote_alias, local_aliases in scheme.items():
        servers[remote_alias] = server_factory.get(remote_alias)  # Create server but don't merge.
        for local_alias, merge_parameters in (local_aliases or {}).items():
            servers[local_alias] = server_factory.get(local_alias)
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
            merge_systems(servers[local_alias], servers[remote_alias], **merge_kwargs)
    return servers
