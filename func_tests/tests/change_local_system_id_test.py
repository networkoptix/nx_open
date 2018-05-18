"""Change local system id. Test servers disconnection if one of them change local system id."""

import logging
import uuid

import server_api_data_generators as generator
from framework.api_shortcuts import get_local_system_id, get_server_id
from framework.waiting import wait_for_true

log = logging.getLogger(__name__)


def check_servers_online(server):
    servers_list = server.api.ec2.getMediaServersEx.GET()
    s_guid = get_server_id(server.api)
    this_servers = [v for v in servers_list if v['id'] == s_guid and v['status'] == 'Online']
    other_servers = [v for v in servers_list if v['id'] != s_guid and v['status'] == 'Online']
    return len(this_servers) == 1 and len(other_servers) != 0


def check_neighbor_offline(server):
    servers_list = server.api.ec2.getMediaServersEx.GET()
    s_guid = get_server_id(server.api)
    this_servers = [v for v in servers_list if v['id'] == s_guid and v['status'] == 'Online']
    other_servers = [v for v in servers_list if v['id'] != s_guid]
    other_offline_servers = [v for v in other_servers if v['status'] == 'Offline']
    return len(this_servers) == 1 and other_servers == other_offline_servers


def test_change_local_system_id(two_merged_linux_mediaservers):
    one, two = two_merged_linux_mediaservers
    wait_for_true(
        lambda: check_servers_online(one),
        "{} and {} should be online".format(one, two),
        timeout_sec=10)
    wait_for_true(
        lambda: check_servers_online(two),
        "{} and {} should be online".format(two, one),
        timeout_sec=10)
    new_local_system_id = generator.generate_server_guid(1, True)
    one.api.api.configure.POST(localSystemId=new_local_system_id)
    assert get_local_system_id(one.api) == uuid.UUID(new_local_system_id)
    wait_for_true(
        lambda: check_neighbor_offline(one),
        "{} should be offline for {}".format(two, one),
        timeout_sec=10)
    wait_for_true(
        lambda: check_neighbor_offline(two),
        "{} should be offline for {}".format(one, two),
        timeout_sec=10)

    assert not one.installation.list_core_dumps()
    assert not two.installation.list_core_dumps()
