from framework.waiting import wait_for_equal


def test_update_info_upload(one_mediaserver_api, update_info):
    one_mediaserver_api.start_update(update_info)
    wait_for_equal(one_mediaserver_api.get_update_information, update_info)
