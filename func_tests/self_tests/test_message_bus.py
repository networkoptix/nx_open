from framework.message_bus import message_bus_running


def test_message_bus(two_merged_mediaservers):
    with message_bus_running(two_merged_mediaservers) as bus:
        bus.wait_until_no_transactions(timeout_sec=1)
