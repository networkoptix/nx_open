import datetime
import logging
import os

import pytest

_logger = logging.getLogger(__name__)


LWS_MERGE_TIMEOUT = datetime.timedelta(minutes=10)


@pytest.mark.skipif('FRAMEWORK_TEST' not in os.environ, reason='Only for testing of testing framework itself')
@pytest.mark.parametrize('iteration', range(2))
def test_merge(linux_mediaservers_pool, lightweight_servers_factory, iteration):
    server = linux_mediaservers_pool.get('full')
    lws_list = lightweight_servers_factory(10)
    _logger.info('Allocated %d lightweight servers', len(lws_list))
    lws_list[0].wait_until_synced(LWS_MERGE_TIMEOUT)
    server.merge_systems(lws_list[0], take_remote_settings=True)

    assert not server.installation.list_core_dumps()
    lightweight_servers_factory.perform_post_checks()


@pytest.mark.skipif('FRAMEWORK_TEST' not in os.environ, reason='Only for testing of testing framework itself')
def test_coredump(lightweight_servers_factory):
    lws_list = lightweight_servers_factory(10)
    lws_list[0].service.make_core_dump()

