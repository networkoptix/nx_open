from datetime import datetime

from pytz import utc

from test_utils.utils import RunningTime


def get_server_id(api):
    return api.get('/ec2/testConnection')['ecsGuid']


def get_system_settings(api):
    settings = api.get('/api/systemSettings')['settings']
    del settings['updateStatus']
    return settings


def get_local_system_id(api):
    return get_system_settings(api)['localSystemId']


def set_local_system_id(api, new_id):
    api.get('/api/configure', params={'localSystemId': new_id})


def get_cloud_system_id(api):
    return get_system_settings(api)['cloudSystemID']


def get_time(api):
    started_at = datetime.now(utc)
    time_response = api.get('/ec2/getCurrentTime')
    received = datetime.fromtimestamp(float(time_response['value']) / 1000., utc)
    return RunningTime(received, datetime.now(utc) - started_at)