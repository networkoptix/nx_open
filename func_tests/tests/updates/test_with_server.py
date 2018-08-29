from framework.waiting import wait_for_true, wait_for_equal


def test_update_info_upload(one_running_mediaserver, update_info):
    one_running_mediaserver.api.start_update(update_info)
    wait_for_equal(one_running_mediaserver.api.get_update_information, update_info)
    assert not one_running_mediaserver.installation.list_core_dumps()


def test_updates_available(one_running_mediaserver, update_info):
    one_running_mediaserver.api.start_update(update_info)
    wait_for_true(
        lambda: one_running_mediaserver.api.get_updates_state() == 'available',
        "{} reports update is available".format(one_running_mediaserver))
    assert not one_running_mediaserver.installation.list_core_dumps()
