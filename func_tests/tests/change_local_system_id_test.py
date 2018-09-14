"""Change local system id. Test servers disconnection if one of them change local system id."""

import logging
import uuid

import server_api_data_generators as generator
from framework.waiting import wait_for_truthy

_logger = logging.getLogger(__name__)


def test_change_local_system_id(two_merged_mediaservers):
    one, two = two_merged_mediaservers
    wait_for_truthy(one.api.servers_is_online, timeout_sec=10)
    wait_for_truthy(two.api.servers_is_online, timeout_sec=10)
    new_local_system_id = generator.generate_server_guid(1, True)
    one.api.set_local_system_id(new_local_system_id)
    assert one.api.get_local_system_id() == uuid.UUID(new_local_system_id)
    wait_for_truthy(one.api.neighbor_is_offline, timeout_sec=10)
    wait_for_truthy(two.api.neighbor_is_offline, timeout_sec=10)
    assert not one.installation.list_core_dumps()
    assert not two.installation.list_core_dumps()
