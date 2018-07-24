import time

from framework.installation.cloud_host_patching import set_cloud_host


# TODO: Reinstall or restore files from .initial before each test.


def test_set_cloud_host(one_mediaserver):
    new_cloud_host_1 = '1.example.com'
    one_mediaserver.stop(already_stopped_ok=True)
    time.sleep(2)  # TODO: Retry on 0xC0000043 in SMBPath.
    set_cloud_host(one_mediaserver.installation, new_cloud_host_1)
    one_mediaserver.start()
    module_information_response_1 = one_mediaserver.api.generic.get('api/moduleInformation')
    assert module_information_response_1['cloudHost'] == new_cloud_host_1
    one_mediaserver.stop()
    time.sleep(2)  # TODO: Retry on 0xC0000043 in SMBPath.
    new_cloud_host_2 = '2.example.com'
    set_cloud_host(one_mediaserver.installation, new_cloud_host_2)
    one_mediaserver.start()
    module_information_response_2 = one_mediaserver.api.generic.get('api/moduleInformation')
    assert module_information_response_2['cloudHost'] == new_cloud_host_2
