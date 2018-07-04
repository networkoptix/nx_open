from framework.timeless_mediaserver import timeless_mediaserver


def test_one_mediaserver(one_mediaserver):
    pass


def test_one_running_mediaserver(one_running_mediaserver):
    pass


def test_two_stopped_mediaservers(two_stopped_mediaservers):
    pass


def test_two_separate_mediaservers(two_separate_mediaservers):
    pass


def test_two_merged_mediaservers(two_merged_mediaservers):
    pass


def test_timeless_mediaserver(one_vm, mediaserver_installers, ca, artifact_factory):
    with timeless_mediaserver(one_vm, mediaserver_installers, ca, artifact_factory) as timeless_server:
        pass
