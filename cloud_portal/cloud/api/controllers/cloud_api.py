import requests
from requests.auth import HTTPDigestAuth
from hashlib import md5
from cloud import settings
from api.helpers.exceptions import validate_response


@validate_response
def ping():
    request = settings.CLOUD_CONNECT['url'] + "/cdb/ping"
    return requests.get(request)


class System(object):
    @staticmethod
    @validate_response
    def list(email, password):
        # TODO: create wrappers
        request = settings.CLOUD_CONNECT['url'] + "/system/get"
        return requests.get(request, auth=HTTPDigestAuth(email, password))

    @staticmethod
    @validate_response
    def get(email, password, system_id):
        # TODO: create wrappers
        request = settings.CLOUD_CONNECT['url'] + "/system/get"
        params = {
           'systemID': system_id
        }
        return requests.get(request, params=params, auth=HTTPDigestAuth(email, password))

    @staticmethod
    @validate_response
    def users(email, password, system_id):
        request = settings.CLOUD_CONNECT['url'] + "/system/get_cloud_users"
        params = {
           'systemID': system_id
        }
        return requests.get(request, params=params, auth=HTTPDigestAuth(email, password))

    @staticmethod
    @validate_response
    def share(email, password, system_id, account_email, role):
        request = settings.CLOUD_CONNECT['url'] + "/system/share"
        params = {
            'systemID': system_id,
            'accountEmail': account_email,
            'accessRole': role
        }
        return requests.post(request, json=params, auth=HTTPDigestAuth(email, password))

    @staticmethod
    @validate_response
    def access_roles(email, password, system_id):
        request = settings.CLOUD_CONNECT['url'] + "/system/get_access_role_list"
        params = {
           'systemID': system_id
        }
        return requests.get(request, params=params, auth=HTTPDigestAuth(email, password))

    @staticmethod
    @validate_response
    def unbind(email, password, system_id):
        request = settings.CLOUD_CONNECT['url'] + "/system/unbind"
        params = {
            'systemID': system_id,
        }
        return requests.post(request, json=params, auth=HTTPDigestAuth(email, password))

    @staticmethod
    @validate_response
    def bind(email, password, name):
        request = settings.CLOUD_CONNECT['url'] + "/system/bind"
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

        params = {
            'email': email,
            'passwordHa1': password_ha1,
            'fullName': ' '.join((first_name, last_name)),
            'customization': customization
        }
        # django.utils.http.urlencode(params)
        request = settings.CLOUD_CONNECT['url'] + '/account/register'

        return requests.post(request, json=params)

    @staticmethod
    @validate_response
    def change_password(email, password, new_password):
        realm = settings.CLOUD_CONNECT['password_realm']

        password_string = ':'.join((email, realm, new_password))
        password_ha1 = md5(password_string).hexdigest()

        params = {
            'passwordHa1': password_ha1
        }
        request = settings.CLOUD_CONNECT['url'] + '/account/update'

        return requests.post(request, json=params, auth=HTTPDigestAuth(email, password))

    @staticmethod
    @validate_response
    def reset_password(user_email):
        params = {
            'email': user_email
        }
        request = settings.CLOUD_CONNECT['url'] + '/account/reset_password'
        return requests.post(request, json=params)

    @staticmethod
    @validate_response
    def activate(code):
        params = {
            'code': code
        }
        request = settings.CLOUD_CONNECT['url'] + '/account/activate'
        return requests.post(request, json=params)

    @staticmethod
    @validate_response
    def reactivate(email):
        params = {
            'email': email
        }
        request = settings.CLOUD_CONNECT['url'] + '/account/reactivate'
        return requests.post(request, json=params)

    @staticmethod
    @validate_response
    def update(email, password, first_name, last_name):

        params = {
            'fullName': ' '.join((first_name, last_name))
        }
        request = settings.CLOUD_CONNECT['url'] + '/account/update'

        return requests.post(request, json=params, auth=HTTPDigestAuth(email, password))

    @staticmethod
    @validate_response
    def get(email=None, password=None):
        # TODO: create wrappers
        request = settings.CLOUD_CONNECT['url'] + "/account/get"
        return requests.get(request, auth=HTTPDigestAuth(email, password))
