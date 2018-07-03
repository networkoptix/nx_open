from framework.timeless_mediaserver import timeless_mediaserver


def test_mediaserver_factory(one_mediaserver):
    pass


def test_timeless_mediaserver(one_vm, mediaserver_installers, ca, artifact_factory):
    with timeless_mediaserver(one_vm, mediaserver_installers, ca, artifact_factory) as timeless_server:
        pass
