import logging
from datetime import timedelta
from typing import Callable, Generator, Optional

from monotonic import monotonic as time_monotomic

from framework.http_api import HttpError
from framework.installation.mediaserver import Mediaserver
from .checks import Failure, Result, Success

_logger = logging.getLogger(__name__)


class Run(object):
    """ Stage execution object, keeps runtime data related to specific stage run on specific camera.
    """
    def __init__(self, stage, server, camera_id):  # type: (Stage, Mediaserver, str) -> None
        self.stage = stage
        self.server = server
        self.id = camera_id
        self.data = None  # type: dict

    def update_data(self):
        self.data = self.server.api.get_resource('CamerasEx', self.id)


class Stage(object):
    """ Stage description object, allows to start execution process for a specific camera.
    """
    def __init__(self, name, actions, is_essential, timeout
                 ):  # type: (str, Callable[[Run], Result], bool, timedelta) -> None
        self.name = name
        self.is_essential = is_essential
        self.timeout = timeout
        self._actions = actions
        assert callable(actions), type(actions)

    def steps(self, server, camera_id, rules
              ):  # type: (Mediaserver, str, dict) -> Generator[Result]
        """ Yields intermediate states:
            - Error - retry is expected in some time if timeout is not expired.
            - Success - stage is successfully finished.
        """
        run = Run(self, server, camera_id)
        actions = self._actions(run, **rules)
        while True:
            try:
                run.update_data()

            except AssertionError:
                yield Failure('Unable to update camera data')

            else:
                yield actions.next()


class Executor(object):
    """ Stage executor for a single camera, executes stage steps and keeps resulting data.
    """
    def __init__(self, camera_id, stage, rules, hard_timeout=None
                 ):  # type: (str, Stage, dict, Optional[timedelta]) -> None
        self.camera_id = camera_id
        self.stage = stage
        self._rules = rules
        self._timeout = min(self.stage.timeout, hard_timeout) if hard_timeout else self.stage.timeout
        self._result = None  # type: Optional[Result]
        self._duration = None

    def steps(self, server):  # type: (Mediaserver) -> Generator[None]
        """ Yields when runner needs some time to wait before stage retry.
            StopIteration means the stage execution is finished, see is_successful.
        """
        steps = self.stage.steps(server, self.camera_id, self._rules)
        start_time = time_monotomic()
        _logger.info('Stage "%s" is started for %s', self.stage.name, self.camera_id)
        while not self._execute_next_step(steps, start_time):
            _logger.debug('Stage "%s" for %s status %s',
                          self.stage.name, self.camera_id, self._result.report)

            if self._duration > self._timeout:
                _logger.info('Stage "%s" for %s timed out', self.stage.name, self.camera_id)
                break

            yield

        _logger.info('Stage "%s" is finished: %s', self.stage.name, self.report)

    @property
    def is_successful(self):
        return isinstance(self._result, Success)

    @property
    def report(self):  # types: () -> dict
        """:returns Current stage execution state, represented as serializable dictionary.
        """
        if not self._result:
            return {}

        data = self._result.report
        if self._duration:
            data['duration'] = str(self._duration)

        return data

    def _execute_next_step(self, stage_steps, start_time
                           ):  # type: (Generator[Result], float) -> bool
        """ :returns True if stage is finished, False otherwise.
        """
        try:
            self._result = stage_steps.next()

        except HttpError as error:
            self._result = Failure(str(error))

        except StopIteration:
            return True

        except Exception:
            self._result = Failure(is_exception=True)
            return True

        finally:
            self._duration = timedelta(seconds=time_monotomic() - start_time)

        if isinstance(self._result, Success):
            return True

        return False
