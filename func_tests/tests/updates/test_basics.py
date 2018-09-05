from framework.waiting import wait_for_equal


def test_update_info_upload(one_running_mediaserver, update_info):
    one_running_mediaserver.api.start_update(update_info)
    wait_for_equal(one_running_mediaserver.api.get_update_information, update_info)
    assert not one_running_mediaserver.installation.list_core_dumps()
