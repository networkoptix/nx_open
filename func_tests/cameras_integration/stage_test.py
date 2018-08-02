import time

import pytest

from .checks import *
from .stage import *

_CAMERA_ID = 'TEST_CAMERA'
_CAMERA_DATA = {'id': _CAMERA_ID, 'name': 'camera_name'}
_STAGE_NAME = 'test_stage'
_STAGE_RULES = {'key1': 'value1', 'key2': 'value2'}


class FakeServer(object):
    class api(object):
        @staticmethod
        def get_resource(path, camera_id):
            assert path == 'CamerasEx'
            if camera_id != _CAMERA_ID:
                raise KeyError(camera_id)
            return _CAMERA_DATA


def make_stage(temporary_results=None, is_success=False, sleep_s=None):
    if temporary_results is None:
        temporary_results = []

    def actions(run, **kwargs):
        assert kwargs == _STAGE_RULES
        assert run.id == _CAMERA_ID
        assert run.data == _CAMERA_DATA
        for result in temporary_results:
            yield result
            if sleep_s:
                time.sleep(sleep_s)

        if is_success:
            yield Success()

    return Stage(_STAGE_NAME, actions, is_essential=False, timeout=timedelta(milliseconds=100))


@pytest.mark.parametrize("temporary_results, is_success", [
    ([], True),
    ([Halt('Wait for it')], True),
    ([Failure('Something went wrong'), Halt('Wait for it')], True),
    ([Failure('Something went wrong')], True),
    ([Halt('Wait for it'), Failure('Something went wrong')], False),
])
def test_execution(temporary_results, is_success):
    executor = Executor(_CAMERA_ID, make_stage(temporary_results, is_success), _STAGE_RULES)
    steps = executor.steps(FakeServer)
    for _ in temporary_results:
        steps.next()

    with pytest.raises(StopIteration):
        steps.next()

    assert is_success == executor.is_successful
    assert executor.report['duration'] > '0:00:00'
    if not is_success:
        for key, value in temporary_results[-1].report.items():
            assert value == executor.report[key]


def test_execution_timeout():
    executor = Executor(_CAMERA_ID, make_stage([Halt('wait')] * 100, sleep_s=0.1), _STAGE_RULES)
    for _ in executor.steps(FakeServer):
        pass

    assert not executor.is_successful
    assert 'wait' == executor.report['message']


def test_execution_exception():
    executor = Executor(_CAMERA_ID, make_stage(), dict(wrong='rules'))
    with pytest.raises(StopIteration):
        executor.steps(FakeServer).next()

    assert not executor.is_successful
    assert 'AssertionError' in '\n'.join(executor.report['exception'])
