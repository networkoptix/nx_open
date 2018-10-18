import logging
import timeit
import json
from datetime import datetime, timedelta

import pytz
from typing import Callable, Generator, Optional

from framework.http_api import HttpError
from framework.installation.mediaserver import Mediaserver
from framework.mediaserver_api import MediaserverApiError, MediaserverApiRequestError
from framework.utils import datetime_utc_now
from .checks import Failure, Halt, Result, Success

_logger = logging.getLogger(__name__)


class Run(object):
    """ Stage execution object, keeps runtime data related to specific stage run on specific camera.
    """
    def __init__(self, stage, server, name, id):
            # type: (Stage, Mediaserver, str, str) -> None
        self.stage = stage
        self.server = server
        self.name = name
        self.id = id
        self._uuid = None  # type: str
        self._data = None  # type: dict

    @property
    def uuid(self):
        self._uuid = self._uuid or (self.data or {}).get('id')
        return self._uuid

    @property
    def data(self):
        try:
            self._data = self._data or self.server.api.get_resource('CamerasEx', self.id)
        except (MediaserverApiError, MediaserverApiRequestError):
            self._data = None
        return self._data

    def media_url(self, profile='primary', position=None):
        url = self.server.api.generic.http.media_url(self.id)
        url += '?stream=' + {'primary': '0', 'secondary': '1'}[profile]
        if position:
            epoch = pytz.utc.localize(datetime.utcfromtimestamp(0))
            time_since_epoch = position - epoch
            url += '&pos=' + str(int(time_since_epoch.total_seconds() * 1000))

        _logger.debug('{} media URL: {}'.format(self.name, url))
        return url

    def clear_cache(self):
        self._data = None


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

    def steps(self, server, camera_name, camera_id, rules):
            # type: (Mediaserver, str, dict) -> Generator[Result]
        """ Yields intermediate states:
            - Error - retry is expected in some time if timeout is not expired.
            - Success - stage is successfully finished.
        """
        run = Run(self, server, camera_name, camera_id)
        if isinstance(rules, list):
            actions = self._actions(run, *rules)
        elif isinstance(rules, dict):
            actions = self._actions(run, **rules)
        else:
            raise TypeError('Unsupported rules type: {}'.format(type(rules)))

        while True:
            run.clear_cache()
            yield actions.next()


class Executor(object):
    """ Stage executor for a single camera, executes stage steps and keeps resulting data.
    """
    def __init__(self, camera_name, camera_id, stage, rules, hard_timeout=None):
            # type: (str, str, Stage, dict, Optional[timedelta]) -> None
        self.camera_name = camera_name
        self.camera_id = camera_id
        self.stage = stage
        self._rules = json.loads(json.dumps(rules))  # Get rid of OrderdDicts
        self._timeout = min(self.stage.timeout, hard_timeout) if hard_timeout else self.stage.timeout
        self._result = None  # type: Optional[Result]
        self._start_time = None
        self._duration = None

    def steps(self, server):  # type: (Mediaserver) -> Generator[None]
        """ Yields when runner needs some time to wait before stage retry.
            StopIteration means the stage execution is finished, see is_successful.
        """
        steps = self.stage.steps(server, self.camera_name, self.camera_id, self._rules)
        self._result = Halt('Stage is not finished')
        self._start_time = datetime_utc_now()
        start_time = timeit.default_timer()
        _logger.info('Stage "%s" is started for %s', self.stage.name, self.camera_name)
        while not self._execute_next_step(steps, start_time):
            _logger.debug(
                'Stage "%s" for %s after %s: %s',
                self.stage.name, self.camera_name, self._duration, self._result)

            if self._duration > self._timeout:
                _logger.info(
                    'Stage "%s" for %s timed out in %s',
                    self.stage.name, self.camera_name, self._duration)
                break

            yield

        steps.close()
        _logger.info(
            'Stage "%s" for %s is finished in %s: %s',
            self.stage.name, self.camera_name, self._duration, self._result)

    @property
    def is_successful(self):
        return isinstance(self._result, Success)

    @property
    def details(self):  # types: () -> dict
        """:returns Current stage execution state.
        """
        if not self._result:
            return {}

        return dict(start_time=self._start_time, duration=self._duration, **self._result.details)

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
            self._duration = timedelta(seconds=timeit.default_timer() - start_time)

        if isinstance(self._result, Success):
            return True

        return False
