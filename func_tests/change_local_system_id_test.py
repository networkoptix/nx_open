'''Change local system id

   It tests servers disconnection if one of them change local system id
'''

import logging

import pytest

import server_api_data_generators as generator
from test_utils.utils import SimpleNamespace

log = logging.getLogger(__name__)


@pytest.fixture
def env(server_factory):
    one = server_factory('one')
    two = server_factory('two')
    one.merge([two])
    return SimpleNamespace(
        one=one,
        two=two,
        )

def check_servers_online(env):
    for s in [env.one, env.two]:
        servers_list = s.rest_api.ec2.getMediaServersEx.GET()
        this_servers = [v for v in servers_list if v['id'] == s.ecs_guid and v['status'] == 'Online']
        other_servers = [v for v in servers_list if v['id'] != s.ecs_guid and v['status'] == 'Online']
        assert len(this_servers) == 1, '%s(%s): should be online in own getMediaServersEx list' % (s.title, s.url)
        assert len(other_servers) != 0, '%s(%s): should be at least one another server in the system' % (s.title, s.url)

def check_servers_offline(env):
    for s in [env.one, env.two]:
        servers_list = s.rest_api.ec2.getMediaServersEx.GET()
        this_servers = [v for v in servers_list if v['id'] == s.ecs_guid and v['status'] == 'Online']
        other_servers = [v for v in servers_list if v['id'] != s.ecs_guid]
        other_offline_servers = [v for v in other_servers if v['status'] == 'Offline']
        assert len(this_servers) == 1, '%s(%s): should be online in own getMediaServersEx list' % (s.title, s.url)
        assert other_servers == other_offline_servers, '%s(%s): all other servers should be offline' % (s.title, s.url)

def test_change_local_system_id(env):
    check_servers_online(env)
    new_local_system_id = generator.generate_server_guid(1, True)
    env.one.rest_api.api.configure.POST(localSystemId=new_local_system_id)
    env.one.load_system_settings()
    assert env.one.local_system_id == new_local_system_id
    check_servers_offline(env)
