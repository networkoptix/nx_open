import logging

from framework.message_bus import MessageBus

_logger = logging.getLogger(__name__)


def test_message_bus(two_merged_mediaservers):
    with MessageBus(two_merged_mediaservers).running() as bus:
        bus.wait_until_no_transactions(timeout_sec=1)
