import pytest

from .checks import *


def _exception_failure(**kwargs):
    try:
        raise KeyError(7)
    except KeyError:
        failure = Failure(is_exception=True, **kwargs)
        failure.exception = failure.exception.split('\n')[-1]  # Remove stack.
        return failure


@pytest.mark.parametrize("result, report", [
    (Success(), {'condition': 'success'}),
    (Failure(errors=['shit', 'happens']), {'condition': 'failure', 'errors': ['shit', 'happens']}),
    (Failure(errors=['shit happens']), {'condition': 'failure', 'errors': 'shit happens'}),
    (Failure(errors=['shit happens']), {'condition': 'failure', 'errors': 'shit happens'}),
    (_exception_failure(), {'condition': 'failure', 'exception': ['KeyError: 7']}),
    (_exception_failure(errors=1), {'condition': 'failure', 'exception': ['KeyError: 7'], 'errors': 1}),
    (Halt('waiting'), {'condition': 'halt', 'message': 'waiting'}),
])
def test_results(result, report):
    assert repr(result.report) == repr(report)


@pytest.mark.parametrize("expected, actual, errors", [
    ({'hello': 'world'}, {'hello': 'world', 'another': 'value'},
        None),
    ({'hello': 'world'}, {'hello': 'worldX'},
        ["'camera.hello' is 'worldX', expected 'world'"]),
    ({'hello': 'world'}, {'helloX': 'worldX'},
        ["'camera.hello' is None, expected 'world'"]),
    ({'top.mid.bottom': 777}, {'top': {'mid': {'bottom': 777}}},
        None),
    ({'top.mid.bottom': 777}, {'top': {'mid': {'bottom': 888}}},
        ["'camera.top.mid.bottom' is 888, expected 777"]),
    ({'top.mid.bottom': 777}, {'top': {'mid': 2}},
        ["'camera.top.mid' is 'int', expected to be a dict"]),
    ({'number1': [5, 10], 'number2': 6}, {'number1': 7, 'number2': 6},
        None),
    ({'number1': [5, 10], 'number2': 6}, {'number1': 3, 'number2': 3},
        ["'camera.number1' is 3, expected to be in [5, 10]", "'camera.number2' is 3, expected 6"]),
    ({'encoderIndex': 'primary', 'codec': 'MJPEG'}, {'encoderIndex': 0, 'codec': 8},
        None),
    ({'encoderIndex': 'primary', 'codec': 'MJPEG'}, {'encoderIndex': 2, 'codec': 28},
        ["'camera.codec' is 'H264', expected 'MJPEG'", "'camera.encoderIndex' is 2, expected 'primary'"]),
    ({'id=b': {'v': 2}}, [{'id': 'a', 'v': 1}, {'id': 'b', 'v': 2}],
        None),
    ({'id=b': {'v': 3}}, [{'id': 'a', 'v': 1}, {'id': 'b', 'v': 2}],
        ["'camera[id=b].v' is 2, expected 3"]),
    ({'id=b': {'v': 3}}, {'id': 'a', 'v': 1},
        ["'camera' is 'dict', expected to be a list"]),
    ({'path.id=a': {'v': 7}}, {'path': [{'id': 'a', 'v': 7}]},
        None),
    ({'path.id=a': {'v': 7}}, {'path': [{'id': 'a', 'v': 6}]},
        ["'camera.path[id=a].v' is 6, expected 7"]),
])
def test_expect_values_success(expected, actual, errors):
    result = expect_values(expected, actual)
    if errors:
        assert isinstance(result, Failure)
        assert set(errors) == set(result.errors)
    else:
        assert isinstance(result, Success)