import logging
import time
from datetime import datetime

from typing import List

from framework.installation.mediaserver import Mediaserver
from . import stage, stages

_logger = logging.getLogger(__name__)


class Camera(object):
    """ Controls camera stages execution flow and provides report.
    """
    def __init__(self, server, camera_id, stage_rules):  # type: (Mediaserver, str, dict) -> None
        self.camera_id = camera_id
        self._stage_executors = self._make_stage_executors(stage_rules)
        self._warnings = ['Unknown stage ' + name for name in stage_rules]
        self._all_stage_steps = self._make_all_stage_steps(server)
        self._duration = None
        _logger.info('Camera %s is added with %s stage(s) and %s warning(s)',
                     camera_id, len(self._stage_executors), len(self._warnings))

    def execute(self):  # types: () -> bool
        """ :returns: True if all stages are finished, False otherwise (retry is required).
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
    def report(self):  # types: () -> dict
        """:returns Current camera stages execution state, represented as serializable dictionary.
        """
        data = dict(
            condition=('success' if self.is_successful else 'failure'),
            duration=str(self._duration) if self._duration else None,
            stages=[dict(_=e.stage.name, **e.report) for e in self._stage_executors if e.report],
            warnings=self._warnings)

        return {k: v for k, v in data.items() if v}

    def _make_all_stage_steps(self, server):  # types: (Mediaserver) -> Generator[None]
        start_time = datetime.now()
        for executors in self._stage_executors:
            steps = executors.steps(server)
            while True:
                try:
                    steps.next()
                    self._duration = datetime.now() - start_time
                    yield

                except StopIteration:
                    _logger.info('%s stage result %s', self.camera_id, executors.report)
                    if not executors.is_successful and executors.stage.is_essential:
                        _logger.error('Essential stage is failed, skip other stages')
                        self._duration = datetime.now() - start_time
                        return
                    break

    def _make_stage_executors(self, stage_rules):  # type: (dict) -> List[stage.Executor]
        executors = []
        for current_stage in stages.LIST:
            try:
                rules = stage_rules.pop(current_stage.name)

            except KeyError:
                if current_stage.is_essential:
                    _logger.warning('Skip camera %s, essential stage "%s" is not configured',
                                    self.camera_id, current_stage.name)
                    return []
            else:
                executors.append(stage.Executor(self.camera_id, current_stage, rules))

        return executors


class Stand(object):
    def __init__(self, server, config):  # type: (Mediaserver, dict) -> None
        self.server = server
        self.server_information = server.api.generic.get('api/moduleInformation')
        self.server_features = server.installation.specific_features
        self.cameras = [Camera(server, i, self._stage_rules(c)) for i, c in config.items()]
        self.start_time = time.time()

    def run_all_stages(self, cycle_delay_s=1):
        _logger.info('Run all stages')
        while True:
            cameras_left = 0
            for camera in self.cameras:
                if not camera.execute():
                    cameras_left += 1

            if not cameras_left:
                _logger.info('Stages execution is finished')
                return

            _logger.debug('Wait for cycle delay %s seconds, %s cameras left',
                          cycle_delay_s, cameras_left)

            time.sleep(cycle_delay_s)


    @property
    def elapsed_time_s(self):
        return time.time() - self.start_time

    @property
    def is_successful(self):
        return all(camera.is_successful for camera in self.cameras)

    @property
    def report(self):  # types: () -> dict
        """:returns All cameras stages execution state, represented as serializable dictionary.
        """
        result = {camera.camera_id: camera.report for camera in self.cameras}
        for camera_id, _ in self.all_cameras().items():
            if camera_id not in result:
                result[camera_id] = dict(condition='failure', message='Unexpected camera on server')

        return result

    def all_cameras(self):
        return {c['physicalId']: c for c in self.server.api.get_resources('CamerasEx')}

    def _stage_rules(self, rules):  # (dict) -> dict
        conditional = {}
        for name, rule in rules.items():
            try:
                base_name, condition = name.split(' if ')
            except ValueError:
                pass
            else:
                del rules[name]
                if eval(condition, dict(features=self.server_features, **self.server_information)):
                    base_rule = conditional.setdefault(base_name.strip(), {})
                    self._merge_stage_rule(base_rule, rule, may_override=False)

        for name, rule in conditional.items():
            base_rule = rules.setdefault(name, {})
            self._merge_stage_rule(base_rule, rule, may_override=True)

        return rules

    @classmethod
    def _merge_stage_rule(cls, destination, source, may_override):
        for key, value in source.items():
            if isinstance(value, dict):
                cls._merge_stage_rule(destination[key], value, may_override)
            else:
                if key in destination and not may_override:
                    raise ValueError('Conflict in stage')
                destination[key] = value
