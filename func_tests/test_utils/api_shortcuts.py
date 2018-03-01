def get_server_id(api):
    return api.get('/ec2/testConnection')['ecsGuid']


def get_settings(api):
    return api.get('/api/systemSettings')['settings']


def get_local_system_id(api):
    return get_settings(api)['localSystemId']


def set_local_system_id(api, new_id):
    api.get('/api/configure', params={'localSystemId': new_id})


def get_cloud_system_id(api):
    return get_settings(api)['cloudSystemID']
