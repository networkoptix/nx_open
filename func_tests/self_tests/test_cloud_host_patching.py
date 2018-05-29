from framework.installation.cloud_host_patching import set_cloud_host


def test_set_cloud_host(one_mediaserver):
    new_cloud_host_1 = '1.example.com'
    one_mediaserver.stop(already_stopped_ok=True)
    set_cloud_host(one_mediaserver.installation, new_cloud_host_1)
    one_mediaserver.start()
    module_information_response_1 = one_mediaserver.api.get('api/moduleInformation')
    assert module_information_response_1['cloudHost'] == new_cloud_host_1
    one_mediaserver.stop()
    new_cloud_host_2 = '2.example.com'
    set_cloud_host(one_mediaserver.installation, new_cloud_host_2)
    one_mediaserver.start()
    module_information_response_2 = one_mediaserver.api.get('api/moduleInformation')
    assert module_information_response_2['cloudHost'] == new_cloud_host_2
