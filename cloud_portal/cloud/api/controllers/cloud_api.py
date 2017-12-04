import requests
from requests.auth import HTTPDigestAuth
from hashlib import md5, sha256
import base64
from cloud import settings
from api.helpers.exceptions import validate_response, ErrorCodes, APIRequestException, APINotAuthorisedException

import logging
logger = logging.getLogger(__name__)

CLOUD_DB_URL = settings.CLOUD_CONNECT['url']


def lower_case_email(func):
    def validator(email, *args, **kwargs):
        email = email.lower()
        return func(email, *args, **kwargs)
    return validator


@validate_response
def ping():
    url = CLOUD_DB_URL + "/ping"
    logger.info('Making ping request to {}'.format(url))
    response = requests.get(url)
    logger.info('Ping request finished')
    return response


class System(object):
    @staticmethod
    @validate_response
    @lower_case_email
    def list(email, password, one_customization=True):
        # TODO: create wrappers
        request = CLOUD_DB_URL + "/system/get"
        if one_customization:
            request += "?customization=" + settings.CUSTOMIZATION
        return requests.get(request, auth=HTTPDigestAuth(email, password))

    @staticmethod
    @validate_response
    @lower_case_email
    def get(email, password, system_id):
        # TODO: create wrappers
        request = CLOUD_DB_URL + "/system/get"
        params = {
           'systemId': system_id
        }
        return requests.get(request, params=params, auth=HTTPDigestAuth(email, password))

    @staticmethod
    @validate_response
    @lower_case_email
    def users(email, password, system_id):
        request = CLOUD_DB_URL + "/system/getCloudUsers"
        params = {
           'systemId': system_id
        }
        return requests.get(request, params=params, auth=HTTPDigestAuth(email, password))

    @staticmethod
    @validate_response
    @lower_case_email
    def share(email, password, system_id, account_email, role):
        account_email = account_email.lower()
        request = CLOUD_DB_URL + "/system/share"
        params = {
            'systemId': system_id,
            'accountEmail': account_email,
            'accessRole': role
        }
        return requests.post(request, json=params, auth=HTTPDigestAuth(email, password))

    @staticmethod
    @validate_response
    @lower_case_email
    def get_nonce(email, password, system_id):
        # TODO: create wrappers
        request = CLOUD_DB_URL + '/auth/getNonce'
        params = {
            'systemId': system_id
        }
        return requests.get(request, params=params, auth=HTTPDigestAuth(email, password))

    @staticmethod
    @validate_response
    @lower_case_email
    def rename(email, password, system_id, system_name):
        request = CLOUD_DB_URL + "/system/rename"
        params = {
            'systemId': system_id,
            'name': system_name
        }
        return requests.post(request, json=params, auth=HTTPDigestAuth(email, password))

    @staticmethod
    @validate_response
    @lower_case_email
    def access_roles(email, password, system_id):
        request = CLOUD_DB_URL + "/system/getAccessRoleList"
        params = {
           'systemId': system_id
        }
        return requests.get(request, params=params, auth=HTTPDigestAuth(email, password))

    @staticmethod
    @validate_response
    @lower_case_email
    def unbind(email, password, system_id):
        request = CLOUD_DB_URL + "/system/unbind"
        params = {
            'systemId': system_id,
        }
        return requests.post(request, json=params, auth=HTTPDigestAuth(email, password))

    @staticmethod
    @validate_response
    @lower_case_email
    def bind(email, password, name):
        request = CLOUD_DB_URL + "/system/bind"
        customization = settings.CLOUD_CONNECT['customization']
        params = {
            'name': name,
            'customization': customization
        }
        return requests.post(request, json=params, auth=HTTPDigestAuth(email, password))

    @staticmethod
    @validate_response
    @lower_case_email
    def merge(email, password, master_system_id, slave_system_id):
        request = CLOUD_DB_URL + "/system/%s/merged_systems/" % master_system_id
        params = {
            'system_id': slave_system_id
        }
        return requests.post(request, json=params, auth=HTTPDigestAuth(email, password))


class Account(object):
    @staticmethod
    def extract_temp_credentials(code):
        try:
            (temp_password, email) = base64.b64decode(code).split(":")
        except TypeError:
            raise APIRequestException('Activation code has wrong structure - TypeError:' + code, ErrorCodes.wrong_code)
        except ValueError:
            raise APIRequestException('Activation code has wrong structure - ValueError:' + code, ErrorCodes.wrong_code)

        if not email or not temp_password:
            raise APIRequestException('Activation code has wrong structure - no email:' + code, ErrorCodes.wrong_code)

        return temp_password, email

    @staticmethod
    @lower_case_email
    def encode_password(email, password):
        realm = settings.CLOUD_CONNECT['password_realm']
        password_string = ':'.join((email, realm, password))
        password_ha1 = md5(password_string).hexdigest()
        password_ha1_sha256 = sha256(password_string).hexdigest()
        return password_ha1, password_ha1_sha256

    @staticmethod
    @lower_case_email
    def register(email, password, first_name, last_name, code=None):
        logger.debug('cloud_api.Account.register: ' + email)

        @validate_response
        def _update(login, password, params):
            request = CLOUD_DB_URL + '/account/update'
            logger.debug('cloud_api.Account.register - making request: ' + request)
            return requests.post(request, json=params, auth=HTTPDigestAuth(login, password))

        @validate_response
        def _register(params):
            request = CLOUD_DB_URL + '/account/register'
            logger.debug('cloud_api.Account.register - making request: ' + request)
            return requests.post(request, json=params)

        customization = settings.CLOUD_CONNECT['customization']
        password_ha1, password_ha1_sha256 = Account.encode_password(email, password)

        params = {
            'email': email,
            'passwordHa1': password_ha1,
            'passwordHa1Sha256': password_ha1_sha256,
            'fullName': ' '.join((first_name, last_name)),
            'customization': customization
        }

        if not code:
            return _register(params)
        else:
            temp_password, code_email = Account.extract_temp_credentials(code)
            if email != code_email:
                raise APIRequestException('Activation code doesn\'t match email:' + code, ErrorCodes.wrong_code)

            try:
                data = _update(code_email, temp_password, params)
            except APINotAuthorisedException:
                raise APIRequestException('Activation code was already used', ErrorCodes.wrong_code)
            return data

    @staticmethod
    def restore_password(code, new_password):
        temp_password, email = Account.extract_temp_credentials(code)
        return Account.change_password(email, temp_password, new_password)

    @staticmethod
    @validate_response
    @lower_case_email
    def change_password(email, password, new_password):
        password_ha1, password_ha1_sha256 = Account.encode_password(email, new_password)
        params = {
            'passwordHa1': password_ha1,
            'passwordHa1Sha256': password_ha1_sha256
        }
        request = CLOUD_DB_URL + '/account/update'

        return requests.post(request, json=params, auth=HTTPDigestAuth(email, password))

    @staticmethod
    @validate_response
    @lower_case_email
    def create_temporary_credentials(email, password, type):
        params = {
            'type': type
        }
        request = CLOUD_DB_URL + '/account/createTemporaryCredentials'
        return requests.post(request, json=params, auth=HTTPDigestAuth(email, password))

    @staticmethod
    @validate_response
    @lower_case_email
    def reset_password(user_email):
        params = {
            'email': user_email
        }
        request = CLOUD_DB_URL + '/account/resetPassword'
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
    @lower_case_email
    def reactivate(email):
        params = {
            'email': email
        }
        request = CLOUD_DB_URL + '/account/reactivate'
        return requests.post(request, json=params)

    @staticmethod
    @validate_response
    @lower_case_email
    def update(email, password, first_name, last_name):
        params = {
            'fullName': ' '.join((first_name, last_name))
        }
        request = CLOUD_DB_URL + '/account/update'
        return requests.post(request, json=params, auth=HTTPDigestAuth(email, password))

    @staticmethod
    @validate_response
    @lower_case_email
    def get(email=None, password=None):
        # TODO: create wrappers
        request = CLOUD_DB_URL + '/account/get'
        return requests.get(request, auth=HTTPDigestAuth(email, password))
