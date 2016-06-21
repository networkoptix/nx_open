import requests
from requests.auth import HTTPDigestAuth
from hashlib import md5, sha256
from cloud import settings
from api.helpers.exceptions import validate_response

CLOUD_DB_URL = settings.CLOUD_CONNECT['url']


@validate_response
def ping():
    request = CLOUD_DB_URL + "/ping"
    return requests.get(request)


class System(object):
    @staticmethod
    @validate_response
    def list(email, password):
        # TODO: create wrappers
        request = CLOUD_DB_URL + "/system/get"
        return requests.get(request, auth=HTTPDigestAuth(email, password))

    @staticmethod
    @validate_response
    def get(email, password, system_id):
        # TODO: create wrappers
        request = CLOUD_DB_URL + "/system/get"
        params = {
           'systemID': system_id
        }
        return requests.get(request, params=params, auth=HTTPDigestAuth(email, password))

    @staticmethod
    @validate_response
    def users(email, password, system_id):
        request = CLOUD_DB_URL + "/system/get_cloud_users"
        params = {
           'systemID': system_id
        }
        return requests.get(request, params=params, auth=HTTPDigestAuth(email, password))

    @staticmethod
    @validate_response
    def share(email, password, system_id, account_email, role):
        request = CLOUD_DB_URL + "/system/share"
        params = {
            'systemID': system_id,
            'accountEmail': account_email,
            'accessRole': role
        }
        return requests.post(request, json=params, auth=HTTPDigestAuth(email, password))

    @staticmethod
    @validate_response
    def access_roles(email, password, system_id):
        request = CLOUD_DB_URL + "/system/get_access_role_list"
        params = {
           'systemID': system_id
        }
        return requests.get(request, params=params, auth=HTTPDigestAuth(email, password))

    @staticmethod
    @validate_response
    def unbind(email, password, system_id):
        request = CLOUD_DB_URL + "/system/unbind"
        params = {
            'systemID': system_id,
        }
        return requests.post(request, json=params, auth=HTTPDigestAuth(email, password))

    @staticmethod
    @validate_response
    def bind(email, password, name):
        request = CLOUD_DB_URL + "/system/bind"
        customization = settings.CLOUD_CONNECT['customization']
        params = {
            'name': name,
            'customization': customization
        }
        return requests.post(request, json=params, auth=HTTPDigestAuth(email, password))


class Account(object):

    @staticmethod
    @validate_response
    def register(email, password, first_name, last_name):
        customization = settings.CLOUD_CONNECT['customization']
        realm = settings.CLOUD_CONNECT['password_realm']

        password_string = ':'.join((email, realm, password))
        password_ha1 = md5(password_string).hexdigest()
        password_ha1_sha256 = sha256(password_string).hexdigest()

        params = {
            'email': email,
            'passwordHa1': password_ha1,
            'passwordHa1Sha256': password_ha1_sha256,
            'fullName': ' '.join((first_name, last_name)),
            'customization': customization
        }
        # django.utils.http.urlencode(params)
        request = CLOUD_DB_URL + '/account/register'

        return requests.post(request, json=params)

    @staticmethod
    @validate_response
    def change_password(email, password, new_password):
        realm = settings.CLOUD_CONNECT['password_realm']

        password_string = ':'.join((email, realm, new_password))
        password_ha1 = md5(password_string).hexdigest()
        password_ha1_sha256 = sha256(password_string).hexdigest()

        params = {
            'passwordHa1': password_ha1,
            'passwordHa1Sha256': password_ha1_sha256
        }
        request = CLOUD_DB_URL + '/account/update'

        return requests.post(request, json=params, auth=HTTPDigestAuth(email, password))

    @staticmethod
    @validate_response
    def create_temporary_credentials(email, password, type):
        params = {
            'type': type
        }
        request = CLOUD_DB_URL + '/account/create_temporary_credentials'
        return requests.post(request, json=params, auth=HTTPDigestAuth(email, password))

    @staticmethod
    @validate_response
    def reset_password(user_email):
        params = {
            'email': user_email
        }
        request = CLOUD_DB_URL + '/account/reset_password'
        return requests.post(request, json=params)

    @staticmethod
    @validate_response
    def activate(code):
        params = {
            'code': code
        }
        request = CLOUD_DB_URL + '/account/activate'
        return requests.post(request, json=params)

    @staticmethod
    @validate_response
    def reactivate(email):
        params = {
            'email': email
        }
        request = CLOUD_DB_URL + '/account/reactivate'
        return requests.post(request, json=params)

    @staticmethod
    @validate_response
    def update(email, password, first_name, last_name):
        params = {
            'fullName': ' '.join((first_name, last_name))
        }
        request = CLOUD_DB_URL + '/account/update'
        return requests.post(request, json=params, auth=HTTPDigestAuth(email, password))

    @staticmethod
    @validate_response
    def get(email=None, password=None):
        # TODO: create wrappers
        request = CLOUD_DB_URL + '/account/get'
        return requests.get(request, auth=HTTPDigestAuth(email, password))
