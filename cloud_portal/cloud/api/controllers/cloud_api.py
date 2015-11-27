import requests
import logging
from requests.auth import HTTPDigestAuth
from hashlib import md5
from cloud import settings
from api.helpers.exceptions import validate_response

logger = logging.getLogger('django')


class System(object):
    @staticmethod
    @validate_response
    def list(email, password):
        # TODO: create wrappers
        request = settings.CLOUD_CONNECT['url'] + "/system/get"

        return requests.get(request, auth=HTTPDigestAuth(email, password))


class Account(object):

    @staticmethod
    @validate_response
    def register(email, password, first_name, last_name):
        customization = settings.CLOUD_CONNECT['customization']
        realm = settings.CLOUD_CONNECT['password_realm']

        password_string = '%s:%s:%s' % (email, realm, password)
        password_ha1 = md5(password_string).hexdigest()

        full_name = '%s %s' % (first_name, last_name)

        params = {
            'email': email,
            'passwordHa1': password_ha1,
            'fullName': full_name,
            'customization': customization
        }
        # django.utils.http.urlencode(params)
        request = settings.CLOUD_CONNECT['url'] + '/account/register'

        return requests.post(request, data=params, auth=HTTPDigestAuth(email, password))

    @staticmethod
    @validate_response
    def change_password(email, password, new_password):
        realm = settings.CLOUD_CONNECT['password_realm']

        password_string = '%s:%s:%s' % (email, realm, new_password)
        password_ha1 = md5(password_string).hexdigest()

        params = {
            'passwordHa1': password_ha1
        }
        request = settings.CLOUD_CONNECT['url'] + '/account/update'

        return requests.post(request, data=params, auth=HTTPDigestAuth(email, password))

    @staticmethod
    @validate_response
    def update(email, password, first_name, last_name):
        full_name = '%s %s' % (first_name, last_name)

        params = {
            'fullName': full_name
        }
        request = settings.CLOUD_CONNECT['url'] + '/account/update'

        return requests.post(request, data=params, auth=HTTPDigestAuth(email, password))

    @staticmethod
    @validate_response
    def get(email=None, password=None):
        # TODO: create wrappers
        request = settings.CLOUD_CONNECT['url'] + "/account/get"

        return requests.get(request, auth=HTTPDigestAuth(email, password))
