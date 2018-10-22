import logging
import time
from datetime import datetime, timedelta
from collections import OrderedDict
from fnmatch import fnmatch

import pytz
from typing import List, Dict

from framework.installation.mediaserver import Mediaserver
from framework.utils import Timer
from . import checks, stage, stages

_logger = logging.getLogger(__name__)

SERVER_STAGES_KEY = 'SERVER_STAGES'


def report(status, start_time=None, duration=None, **details
           ):  # types: (object, datetime, timedelta, dict) -> OrderedDict
    if isinstance(status, bool):
        status = 'success' if status else 'failure'
    result = OrderedDict(status=str(status))
    if start_time:
        result['start_time'] = str(start_time)
    if duration:
        result['duration'] = str(duration)
    result.update(**{key: value for key, value in details.items() if value})
    return result


class CameraStagesExecutor(object):
    """ Controls camera stages execution flow and provides report.
    """
    def __init__(self, server, name, stage_rules, stage_hard_timeout):
            # type: (Mediaserver, str, dict, timedelta) -> None
        self.name = name
        self.id = stage_rules.get('discovery', {}).get('physicalId')
        self._stage_executors = self._make_stage_executors(stage_rules, stage_hard_timeout)
        self._warnings = ['Unknown stage ' + name for name in stage_rules]
        self._all_stage_steps = self._make_all_stage_steps(server)
        self._start_time = None
        self._duration = None
        _logger.info('New %s', self)

    def __repr__(self):
        return '<{type_name} for {name}({id}) with {stages} stages, {warnings} warnings>'.format(
            type_name=type(self).__name__, stages=len(self._stage_executors),
            warnings=len(self._warnings), **self.__dict__)

    def execute(self):  # types: () -> bool
        """ :returns True if all stages are finished, False otherwise (retry is required).
        """
        try:
            self._all_stage_steps.next()
            return False

        except StopIteration:
            return True

    @property
    def is_successful(self):
        return not self._warnings and all(r.is_successful for r in self._stage_executors)

    @property
    def report(self):  # types: () -> OrederdDict
        """ :returns Current camera stages execution state, represented as serializable dictionary.
        """
        return report(
            self.is_successful, self._start_time, self._duration,
            warnings=self._warnings,
            stages=OrderedDict(
                (e.stage.name, report(**e.details)) for e in self._stage_executors if e.details
            ))

    def _make_all_stage_steps(self, server):  # types: (Mediaserver) -> Generator[None]
        self._start_time = datetime.now(pytz.UTC)
        timer = Timer()
        for executors in self._stage_executors:
            steps = executors.steps(server)
            while True:
                try:
                    steps.next()
                    self._duration = timer.duration
                    yield

                except StopIteration:
                    _logger.info('%s stages result %s', self.name, executors.details)
                    if not executors.is_successful and executors.stage.is_essential:
                        _logger.error('Essential stage is failed, skip other stages')
                        self._duration = timer.duration
                        return
                    break

    def _make_stage_executors(self, stage_rules, hard_timeout):
            # type: (dict, timedelta) -> List[stage.Executor]
        executors = []
        for current_stage in stages.LIST:
            try:
                rules = stage_rules.pop(current_stage.name)

            except KeyError:
                if current_stage.is_essential:
                    _logger.warning(
                        'Skip camera %s, essential stage "%s" is not configured',
                        self.name, current_stage.name)
                    stage_rules.clear()
                    return []
            else:
                executors.append(stage.Executor(
                    self.name, self.id, current_stage, rules, hard_timeout))

        return executors


class ServerStagesExecutor(object):
    class Stage(object):
        def __init__(self, name, rules):  # types: (str, dict) -> None
            self.name = name
            self.rules = rules
            self.result = checks.Halt('Is not executed')
            self.start_time = datetime.now(pytz.UTC)
            self.duration = None

        @property
        def details(self):
            return dict(start_time=self.start_time, duration=self.duration, **self.result.details)

    def __init__(self, server, stage_rules={}):  # types: (Mediaserver, dict) -> None
        self.server = server
        self.rules = stage_rules
        self.stages = []

    def run(self, name, delay=None):  # types: (str, timedelta) -> None
        rules = self.rules.get(name)
        if not rules:
            return

        if delay:
            _logger.debug(self, 'Server stage %r delay %s', name, delay)
            time.sleep(delay.seconds)

        _logger.debug(self, 'Server stage %r', name)
        current_stage = self.Stage(name, rules)
        checker = checks.Checker()
        timer = Timer()
        try:
            for query, expected_values in current_stage.rules.items():
                actual_values = self.server.api.generic.get(query)
                checker.expect_values(expected_values, actual_values, '<{}>'.format(query))

        except Exception:
            current_stage.result = checks.Failure.from_current_exception()

        else:
            current_stage.result = checker.result()

        current_stage.duration = timer.duration
        self.stages.append(current_stage)
        _logger.info(self, 'Server stage %r result %r', name, current_stage.result.details)

    @property
    def is_successful(self):
        return all(isinstance(s.result, checks.Success) for s in self.stages)

    @property
    def report(self):  # types: () -> dict
        """:returns Current server stages execution state, represented as serializable dictionary.
        """
        core_dumps = self.server.installation.list_core_dumps()
        return report(
            self.is_successful and not core_dumps,
            start_time=(self.stages[0].start_time if self.stages else None),
            duration=sum((s.duration for s in self.stages), timedelta()),
            crashes=[dump.name for dump in core_dumps],
            stages=OrderedDict((s.name, report(**s.details)) for s in self.stages))


class SpecificFeatures(object):
    def __init__(self, items=[]):
        self.items = set(items)

    def __getattr__(self, name):
        return name in self.items


class Stand(object):
    def __init__(self, server, config, stage_hard_timeout=timedelta(hours=1), camera_filters=['*']):
            # type: (Mediaserver, Dict[str, dict], timedelta, List[str]) -> None
        self.server = server
        self.server_information = server.api.generic.get('api/moduleInformation')
        self.server_features = server.installation.specific_features()
        self.server_stages = ServerStagesExecutor(server, self._stage_rules(
            config.pop(SERVER_STAGES_KEY) if SERVER_STAGES_KEY in config else {}
        ))
        self.camera_stages = [
            CameraStagesExecutor(server, name, self._stage_rules(config_rules), stage_hard_timeout)
            for name, config_rules in config.items()
            if any(fnmatch(name, f) for f in camera_filters)
        ]

    def run_all_stages(self, camera_cycle_delay, server_stage_delay):
            # types: (timedelta, timedelta) -> None
        _logger.info(
            'Stand run with %s cameras and %s server stages',
            len(self.camera_stages), len(self.server_stages.stages))
        self.server_stages.run('before')
        self._run_camera_stages(camera_cycle_delay)
        self.server_stages.run('after')
        self.server_stages.run('delayed', delay=server_stage_delay)
        _logger.info(
            'Stand run camera success rate %s/%s, server stages result: %s',
            sum(1 if c.is_successful else 0 for c in self.camera_stages), len(self.camera_stages),
            self.server_stages.is_successful)

    @property
    def is_successful(self):
        return all(c.is_successful for c in self.camera_stages) and self.server_stages.is_successful

    @property
    def report(self):  # types: () -> dict
        """:returns All cameras stages execution state, represented as serializable dictionary.
        """
        result = OrderedDict()
        result[SERVER_STAGES_KEY] = self.server_stages.report
        for camera in self.camera_stages:
            result[camera.name] = camera.report

        test_camera_ids = set(camera.id for camera in self.camera_stages)
        for name, description in self.all_cameras(verbose=False).items():
            if description['physicalId'] not in test_camera_ids:
                result[name] = report(
                    'unexpected', description=description, message='Unexpected camera on server')

        _logger.debug('Report: %r', result)
        return result

    def all_cameras(self, verbose):
        return {
            '{vendor}_{model}_{physicalId}'.format(**description).replace(' ', '_'): description
            for description in self.server.api.get_resources('CamerasEx' if verbose else 'Cameras')
        }

    def _run_camera_stages(self, cycle_delay):  # types: (timedelta) -> None
        _logger.info('Run all stages')
        while True:
            cameras_left = 0
            for camera in self.camera_stages:
                if not camera.execute():
                    cameras_left += 1

            if not cameras_left:
                _logger.info('All camera stages execution is finished')
                return

            _logger.debug('Wait for cycle delay %s, %s cameras left', cycle_delay, cameras_left)
            time.sleep(cycle_delay.total_seconds())

    def _stage_rules(self, rules):  # (dict) -> dict
        for name, rule in rules.items():
            try:
                base_name, condition = name.split(' if ')
                base_name = base_name.strip()

            except ValueError:
                pass

            else:
                del rules[name]
                if eval(condition, dict(features=SpecificFeatures(
                        self.server_features), **self.server_information)):
                    self._merge_dict(rules, base_name, rule)

        return rules

    @classmethod
    def _merge_dict(cls, container, key, value):
        if not isinstance(value, dict):
            container[key] = value
        else:
            for sub_key, sub_value in value.items():
                cls._merge_dict(container.setdefault(key, {}), sub_key, sub_value)
