"""
This file contains all test stages, implementation rules are simple:

Exception is thrown = Failure, exception stack is returned.
Success result = Success is returned.
Failure result = Error is saved, retry will fallow.
Halt = Next iteration will fallow.
Stop iteration = Failure, last error is returned.
"""

from typing import Generator, List

from framework.camera import Camera
from . import stage
from .checks import Checker, Failure, Halt, Result, Success, expect_values

# Filled by _stage decorator.
LIST = []  # type: List[stage.Stage]


def _stage(is_essential=False, timeout_s=30, timeout_m=None):
    """:param is_essential - if True and stage is failed then no other stages will be executed.
    """
    if timeout_m is not None:
        timeout_s = timeout_m * 60

    def decorator(actions):
        new_stage = stage.Stage(actions.__name__, actions, is_essential, timeout_s)
        LIST.append(new_stage)
        return new_stage

    return decorator


@_stage(is_essential=True, timeout_m=2)
def discovery(run, **kwargs):  # type: (stage.Run) -> Generator[Result]
    if 'mac' not in kwargs:
        kwargs['mac'] = run.id

    if 'name' not in kwargs:
        kwargs['name'] = kwargs['model']

    while True:
        yield expect_values(kwargs, run.data)


@_stage(is_essential=True, timeout_m=2)
def authorization(run, password, login=None):  # type: (stage.Run, str, str) -> Generator[Result]
    if password != 'auto':
        run.server.api.set_camera_credentials(run.data['id'], login, password)
        yield Halt('Try to set credentials')

    checker = Checker()
    while not checker.expect_values(dict(status="Online"), run.data):
        yield checker.result()

    yield Success()


@_stage()
def recording(run, **options):  # type: (stage.Run) -> Generator[Result]
    camera = Camera(None, None, run.data['name'], run.data['mac'], run.data['id'])
    run.server.api.start_recording_camera(camera, options=options)
    yield Halt('Try to start recording')

    checker = Checker()
    while not checker.expect_values(dict(status="Recording"), run.data):
        yield checker.result()

    if not run.server.api.get_recorded_time_periods(camera):
        # TODO: Verify recorded periods and try to pull video data.
        yield Failure('No data is recorded')

    yield Success()


@_stage(timeout_m=10)
def attributes(self, **kwargs):  # type: (stage.Run) -> Generator[Result]
    while True:
        yield expect_values(kwargs, self.data)
