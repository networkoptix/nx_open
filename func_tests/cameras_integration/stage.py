import logging
import time

from typing import Callable, Generator

import checks
from framework.installation.mediaserver import Mediaserver
from framework.rest_api import HttpError

_logger = logging.getLogger(__name__)


class Run(object):
    def __init__(self, stage, server, camera_id):  # type: (Stage, Mediaserver, str) -> None
        self.stage = stage
        self.server = server
        self.id = camera_id
        self.data = None  # type: dict

    def update_data(self):
        self.data = self.server.get_resource('CamerasEx', self.id)


class Stage(object):
    def __init__(self, name, actions, is_essential, timeout_s
                 ):  # type: (str, Callable[[Run], checks.Result], bool, int) -> None
        self.name = name
        self.is_essential = is_essential
        self.timeout_s = timeout_s
        self._actions = actions
        assert callable(actions), type(actions)

    def steps(self, server, camera_id, rules
              ):  # type: (Mediaserver, str, dict) -> Generator[checks.Result]
        run = Run(self, server, camera_id)
        actions = self._actions(run, **rules)
        while True:
            try:
                run.update_data()

            except AssertionError:
                yield checks.Result('Unable to update camera data')

            else:
                yield actions.next()


class Runner(object):
    def __init__(self, camera_id, stage, rules):  # type: (str, Stage, dict) -> None
        self.camera_id = camera_id
        self.stage = stage
        self._rules = rules
        self._result = None
        self._duration = None

    def steps(self, server):  # type: (Mediaserver) -> Generator[None]
        self._result = checks.Result('No result')
        steps = self.stage.steps(server, self.camera_id, self._rules)
        start_time_s = time.time()
        # TODO: introduce stage timeout and use it!!
        while not self._step(steps):
            _logger.debug('Stage "{}" for {} status {}'.format(
                self.stage.name, self.camera_id, self._result.report()))

            if start_time_s + self.stage.timeout_s < time.time():
                _logger.info('Stage "{}" for {} timeout'.format(self.stage.name, self.camera_id))
                break

            yield

        self._duration = '{} seconds'.format(time.time() - start_time_s)

    @property
    def is_successful(self):
        return self._result.is_successful

    def report(self):  # types: () -> dict
        data = dict()
        if self._duration:
            data['duration'] = self._duration
        if self._result:
            data['status'] = 'success' if self.is_successful else 'failure'
            data.update(self._result.report())
        return data

    def _step(self, steps):  # type: (Generator[checks.Result]) -> bool
        try:
            result = steps.next()
            if result is not None:
                self._result = result

        except HttpError as error:
            self._result = checks.Result(str(error))

        except StopIteration:
            return True

        except Exception:
            self._result = checks.Result(is_exception=True)
            return True

        if self._result and self._result.is_successful:
            return True

        return False
