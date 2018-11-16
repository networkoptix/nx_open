import time
import logging

from framework.mediaserver_api import MediaserverApi

logging.basicConfig(level=logging.DEBUG, format='%(asctime)-15s %(message)s')
_logger = logging.getLogger()

SERVER_URL = 'http://admin:qweasd12345@127.0.0.1:7021'
SOURCE = 'some_source'
CAM_EVENT_TYPES = [
    'cameraMotionEvent',
    'cameraInputEvent',
    'cameraDisconnectEvent',
    'networkIssueEvent',
    'cameraIpConflictEvent',
    'softwareTriggerEvent',
    'userDefinedEvent',
    ]

SERVER_EVENT_TYPES = [
    'undefinedEvent',
    'storageFailureEvent',
    'serverFailureEvent',
    'serverConflictEvent',
    'serverStartEvent',
    'licenseIssueEvent',
    'backupFinishedEvent',
    ]

EVENT_TYPES = CAM_EVENT_TYPES + SERVER_EVENT_TYPES

server = MediaserverApi(SERVER_URL)
existing_rules = server.get_event_rules()

for event_type in EVENT_TYPES:
    rule_existence_flag = False
    for rule in existing_rules:
        if event_type == rule['eventType'] and rule['actionType'] == 'showPopupAction':
            rule_existence_flag = True

    if event_type in CAM_EVENT_TYPES:
        event_resource_id = server.all_cameras[0].camera_id.strip('{}')
    elif event_type in SERVER_EVENT_TYPES:
        event_resource_id = server.get_server_id().strip('{}')

    if not rule_existence_flag:
        _logger.debug(
            server.make_event_rule(
                event_type, 'undefined', 'showPopupAction', event_resource_id, allUsers=True))
    time.sleep(0.25)
    params = {'event_type': event_type, 'eventResourceId': event_resource_id, 'source': SOURCE}
    _logger.debug(server.create_event(**params))
