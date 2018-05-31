import pytest

from framework.pool import CachingPool


class _CallRecordingDict(object):
    def __init__(self, data):
        self._data = data
        self.calls = []

    def get(self, key):
        self.calls.append(key)
        return self._data[key]


@pytest.fixture()
def call_recording_pool():
    return _CallRecordingDict({'mocked created key': 'mocked created value'})


@pytest.fixture()
def caching_pool(call_recording_pool):
    return CachingPool(call_recording_pool, {})


@pytest.fixture()
def caching_pool_preloaded(call_recording_pool):
    return CachingPool(call_recording_pool, {'created beforehand key': 'created beforehand value'})


def test_caching_pool(caching_pool, call_recording_pool):
    created = caching_pool.get('mocked created key')
    assert call_recording_pool.calls == ['mocked created key']
    assert created == 'mocked created value'
    cached = caching_pool.get('mocked created key')
    assert call_recording_pool.calls == ['mocked created key']
    assert cached is created


def test_caching_pool_preload(caching_pool_preloaded, call_recording_pool):
    cached = caching_pool_preloaded.get('created beforehand key')
    assert call_recording_pool.calls == []
    assert cached == 'created beforehand value'
