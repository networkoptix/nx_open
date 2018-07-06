"""Change local system id. Test servers disconnection if one of them change local system id."""

import logging

import server_api_data_generators as generator
from framework.api_shortcuts import get_local_system_id, get_server_id

log = logging.getLogger(__name__)


def check_servers_online(one, two):
    for s in [one, two]:
        servers_list = s.api.ec2.getMediaServersEx.GET()
        s_guid = get_server_id(s.api)
        this_servers = [v for v in servers_list if v['id'] == s_guid and v['status'] == 'Online']
        other_servers = [v for v in servers_list if v['id'] != s_guid and v['status'] == 'Online']
        assert len(this_servers) == 1, '%s(%s): should be online in own getMediaServersEx list' % (s.title, s.url)
        assert len(other_servers) != 0, '%s(%s): should be at least one another server in the system' % (s.title, s.url)


def check_servers_offline(one, two):
    for s in [one, two]:
        servers_list = s.api.ec2.getMediaServersEx.GET()
        s_guid = get_server_id(s.api)
        this_servers = [v for v in servers_list if v['id'] == s_guid and v['status'] == 'Online']
        other_servers = [v for v in servers_list if v['id'] != s_guid]
        other_offline_servers = [v for v in other_servers if v['status'] == 'Offline']
        assert len(this_servers) == 1, '%s(%s): should be online in own getMediaServersEx list' % (s.title, s.url)
        assert other_servers == other_offline_servers, '%s(%s): all other servers should be offline' % (s.title, s.url)


def test_change_local_system_id(one, two):
    check_servers_online(one, two)
    new_local_system_id = generator.generate_server_guid(1, True)
    one.api.api.configure.POST(localSystemId=new_local_system_id)
    assert get_local_system_id(one.api) == new_local_system_id
    check_servers_offline(one, two)

    assert not one.installation.list_core_dumps()
    assert not two.installation.list_core_dumps()
