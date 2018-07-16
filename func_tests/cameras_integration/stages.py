"""
This file contains all test stages, implementation rules are simple:

Exception is thrown = Failure, exception stack is returned.
Successful result = Success is returned.
Unsuccessful result = Error is saved, retry will fallow.
Empty result = Halt, next iteration will fallow.
Stop iteration = Failure, last error is returned.
"""

from typing import List

import checks
import stage
from framework.camera import Camera

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
def discovery(run, **kwargs):  # type: (stage.Run) -> None
    if 'mac' not in kwargs:
        kwargs['mac'] = kwargs['physicalId']

    if 'name' not in kwargs:
        kwargs['name'] = kwargs['model']

    while True:
        yield checks.expect_values(kwargs, run.data)


@_stage(is_essential=True, timeout_m=1)
def authorization(run, password, login=None):  # type: (stage.Run, str, str) -> None
    if password != 'auto':
        run.server.set_camera_credentials(run.data['id'], login, password)
        yield checks.Result('Try to set credentials')

    checker = checks.Checker()
    while not checker.expect_values(dict(status="Online"), run.data):
        yield checker.result()

    yield checks.Result()


@_stage()
def recording(run, **options):  # type: (stage.Run) -> None
    camera = Camera(None, None, run.data['name'], run.data['mac'], run.data['id'])
    run.server.start_recording_camera(camera, options=options)
    yield checks.Result('Try to start recording')

    checker = checks.Checker()
    while not checker.expect_values(dict(status="Recording"), run.data):
        yield checker.result()

    if not run.server.get_recorded_time_periods(camera):
        # TODO: Verify recorded periods and try to pull video data.
        yield checks.Result('No data is recorded')

    yield checks.Result()


@_stage()
def attributes(self, **kwargs):  # type: (stage.Run) -> None
    while True:
        yield checks.expect_values(kwargs, self.data)
