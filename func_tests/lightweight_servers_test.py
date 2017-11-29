import datetime
import logging
import os

import pytest

log = logging.getLogger(__name__)


LWS_MERGE_TIMEOUT = datetime.timedelta(minutes=10)


@pytest.mark.skipif(not os.environ.has_key('FRAMEWORK_TEST'), reason='Only for testing of testing framework itself')
@pytest.mark.parametrize('iteration', range(2))
def test_merge(server_factory, lightweight_servers_factory, iteration):
    server = server_factory('full')
    lws_list = lightweight_servers_factory(10)
    log.info('Allocated %d lightweight servers', len(lws_list))
    lws_list[0].wait_until_synced(LWS_MERGE_TIMEOUT)
    server.merge_systems(lws_list[0], take_remote_settings=True)

@pytest.mark.skipif(not os.environ.has_key('FRAMEWORK_TEST'), reason='Only for testing of testing framework itself')
def test_coredump(lightweight_servers_factory):
    lws_list = lightweight_servers_factory(10)
    lws_list[0].make_core_dump()

