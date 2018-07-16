import logging
import time

from typing import List

import stage
import stages
from framework.installation.mediaserver import Mediaserver

_logger = logging.getLogger(__name__)


class Camera(object):
    def __init__(self, server, camera_id, stage_rules):  # type: (Mediaserver, str, dict) -> None
        self.camera_id = camera_id
        self._runners = self._make_runners(stage_rules)
        self._warnings = ['Unknown stage ' + name for name in stage_rules]
        self._steps = self._make_steps(server)
        _logger.info('Camera {} is added with {} stage(s) and {} warning(s)'.format(
            camera_id, len(self._runners), len(self._warnings)))

    def execute(self):  # types: () -> bool
        try:
            self._steps.next()
            return False

        except StopIteration:
            return True

    @property
    def is_successful(self):
        return not self._warnings and all(r.is_successful for r in self._runners)

    def report(self):  # types: () -> dict
        data = dict(
            status=('success' if self.is_successful else 'failure'),
            stages=[dict(_=runner.stage.name, **runner.report()) for runner in self._runners],
            warnings=self._warnings)
        return {k: v for k, v in data.items() if v}

    def _make_steps(self, server):  # types: (Mediaserver) -> Generator[None]
        for runner in self._runners:
            steps = runner.steps(server)
            while True:
                try:
                    steps.next()
                    yield

                except StopIteration:
                    _logger.info('{} stage result {}'.format(self.camera_id, runner.report()))
                    if not runner.is_successful and runner.stage.is_essential:
                        _logger.error('Essential stage is failed, skip other stages')
                        return
                    break

    def _make_runners(self, stage_rules):  # type: (dict) -> List[stage.Runner]
        runners = []
        for current_stage in stages.LIST:
            try:
                rules = stage_rules.pop(current_stage.name)

            except KeyError:
                if current_stage.is_essential:
                    _logger.warning('Skip camera {}, essential stage "{}" is not configured'.format(
                        self.camera_id, current_stage.name))

                    return []
            else:
                runners.append(stage.Runner(self.camera_id, current_stage, rules))

        return runners


class Stand(object):
    def __init__(self, server, config):  # type: (Mediaserver, dict) -> None
        self.server_information = server.api.get('api/moduleInformation')
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
                return

            _logger.debug('Wait for cycle delay {} seconds, {} cameras left'.format(
                cycle_delay_s, cameras_left))

            time.sleep(cycle_delay_s)

    @property
    def elapsed_time_s(self):
        return time.time() - self.start_time

    @property
    def is_successful(self):
        return all(camera.is_successful for camera in self.cameras)

    def report(self):  # types: () -> dict
        return {camera.camera_id: camera.report() for camera in self.cameras}

    def _stage_rules(self, rules):  # (dict) -> dict
        conditional = {}
        for name, rule in rules.items():
            try:
                base_name, condition = name.split(' if ')
            except ValueError:
                pass
            else:
                del rules[name]
                if eval(condition, self.server_information):
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
