def test_empty():
    pass


def test_online(one_mediaserver_api):
    assert one_mediaserver_api.is_online()
