import yaml
import os
import time
import logging
from urlparse import urlparse

from framework.installation.make_installation import make_installation
from framework.installation.mediaserver import Mediaserver
from framework.os_access.local_access import local_access

import verifications

DISCOVERY_IP_PREFIX = '192.168'  # < TODO: Remove in production.
DISCOVERY_RETRY_COUNT = 10
DISCOVERY_RETRY_DELAY_S = 10

# TODO: Pass this file by test paramiters.
with open(os.path.dirname(__file__) + '/expected_cameras.yaml') as f:
    EXPECTED_CAMERAS = yaml.load(f)


# TODO: Make this test work with fresh configured server on VM with bridged interface.
def test_cameras(mediaserver_installers):
    installation = make_installation(mediaserver_installers, 'linux', local_access)
    server = Mediaserver(local_access, installation, password='qweasd123')
    time.sleep(1)  # TODO: Remove when server is integrated as normal test.

    stand = Stand(server)
    stand.discover_cameras()
    stand.execute_verification_stages()

    # TODO: Write results somewhere on HDD.
    print '-' * 80
    print(yaml.safe_dump(stand.result, default_flow_style=False))
    assert stand.is_success


class Camera(object):
    def __init__(self, data=None, is_expected=False):
        self.data = data
        self.is_expected = is_expected
        self.stages = {}
        self.has_essential_failure = False

    def __nonzero__(self):
        return self.data and self.is_expected and all(self.stages.itervalues())

    def __repr__(self):
        return repr(self.to_dict())

    @property
    def id(self):
        return self.data['id']

    def to_dict(self):
        d = dict(status=('ok' if self else 'error'), id=self.id)
        if not self.data:
            d['description'] = "Is not discovered"
        elif not self.is_expected:
            d['description'] = "Is not expected"
            d['data'] = self.data
        elif self.stages:
            stages = []
            for stage in verifications.STAGES:
                result = self.stages.get(stage.name)
                if result is not None:
                    stages.append(dict(_=stage.name, **result.to_dict()))
            d['stages'] = stages
        return d


class RetryWithDelay(object):
    def __init__(self, retry_count, retry_delay_s):
        self.attempts = 0
        self.retry_count = retry_count
        self.retry_delay_s = retry_delay_s

    def next_try(self):
        self.attempts += 1
        if self.attempts == 1:
            return True
        if self.attempts >= self.retry_count:
            return False
        time.sleep(self.retry_delay_s)
        return True


class Stand(object):
    def __init__(self, server):
        self.server = server
        self.cameras = {}

    @property
    def result(self):
        return {ip: camera.to_dict() for ip, camera in self.cameras.iteritems()}

    @property
    def is_success(self):
        return all(camera for camera in self.cameras.itervalues())

    def discover_cameras(self):
        logging.info('Expected cameras: ' + ', '.join(EXPECTED_CAMERAS.keys()))
        retry = RetryWithDelay(DISCOVERY_RETRY_COUNT, DISCOVERY_RETRY_DELAY_S)
        while retry.next_try():
            discovered_cameras = {}
            for camera in self.server.get_cameras():
                ip = urlparse(camera['url']).hostname
                if ip.startswith(DISCOVERY_IP_PREFIX):
                    discovered_cameras[ip] = camera

            logging.info('Discovered cameras: ' + ', '.join(discovered_cameras))
            if all(ip in discovered_cameras for ip in EXPECTED_CAMERAS):
                break  # < Found everything what was expected.

        for ip in EXPECTED_CAMERAS:
            self.cameras[ip] = Camera(discovered_cameras.get(ip), is_expected=True)

        for ip, data in discovered_cameras.iteritems():
            if ip not in EXPECTED_CAMERAS:
                self.cameras[ip] = Camera(data, is_expected=False)

    def execute_verification_stages(self):
        for stage in verifications.STAGES:
            retry = RetryWithDelay(stage.retry_count, stage.retry_delay_s)
            while retry.next_try():
                if self._execute_verification_stage(stage):
                    break

            if stage.is_essential:
                for camera in self.cameras.itervalues():
                    result = camera.stages.get(stage.name)
                    if result is not None and not result:
                        camera.has_essential_failure = True


    def _execute_verification_stage(self, stage):
        error_count = 0
        for ip, camera in self.cameras.iteritems():
            if camera.has_essential_failure:
                continue  # < Does not make sense to verify anything else.

            if camera.stages.get(stage.name):
                continue  # < This stage is already passed.

            stage_expect = EXPECTED_CAMERAS.get(ip, {}).get(stage.name)
            if stage_expect is None:
                continue  # < This stage is not expected for this camera.

            logging.debug('Camera {}, stage {}'.format(ip, stage.name))
            result = stage(self.server, camera.id, **stage_expect)
            if not result:
                error_count += 1

            logging.debug('Camera {}, stage {}, result: {}'.format(ip, stage.name, result.to_dict()))
            camera.stages[stage.name] = result

        return not error_count
