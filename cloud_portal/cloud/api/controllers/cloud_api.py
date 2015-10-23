__author__ = 'noptix'

from requests.auth import HTTPDigestAuth
from hashlib import md5
from  cloud import settings
import logging
import requests
import django

logger = logging.getLogger('django')

class system(object):
    @staticmethod
    def list(email, password):
         # TODO: create wrappers
        request = settings.CLOUD_CONNECT['url'] + "/system/get"

        response = requests.get(request, auth=HTTPDigestAuth(email, password))

        logger.debug(request)
        logger.debug(response.status_code)
        logger.debug(response.text)
        logger.debug('------------------------------------------------------------')

        return response.json()


class account(object):

    @staticmethod
    def register(email, password, first_name, last_name):
        '''
            <function>
                <name>register</name>
                <description></description>
                <method>GET</method>
                <params>
                    <param>
                        <name>email</name>
                        <description>Unique address to be associated with account</description>
                        <optional>false</optional>
                    </param>
                    <param>
                        <name>passwordHa1</name>
                        <description>Hex representation of HA1 (see rfc2617) digest of user's password. Realm is VMS</description>
                        <optional>true</optional>
                    </param>
                    <param>
                        <name>fullName</name>
                        <description></description>
                        <optional>true</optional>
                    </param>
                    <param>
                        <name>customization</name>
                        <description>Customization of portal has been used to create account</description>
                        <optional>true</optional>
                    </param>
                </params>
                <result>
                    <caption>Account activation code in JSON format</caption>
                </result>
            </function>
        '''


        customization = settings.CLOUD_CONNECT['customization']
        realm = settings.CLOUD_CONNECT['password_realm']
        api_url = settings.CLOUD_CONNECT['url']

        passwordString = '%s:%s:%s' % (email, realm, password)
        passwordHA1 = md5(passwordString).hexdigest()


        full_name = '%s %s' % (first_name, last_name)


        request = '%s/account/register?%s' % (api_url, django.utils.http.urlencode({
            'email':email,
            'passwordHa1':passwordHA1,
            'fullName':full_name,
            'customization':customization
        }))


        response = requests.get(request)

        logger.debug({"passwordString":passwordString,"passwordHA1":passwordHA1})
        logger.debug(request)
        logger.debug(response.status_code)
        logger.debug(response.text)
        logger.debug('------------------------------------------------------------')


        if response.status_code != 200:
            return None

        code = response.json()
        # REMOVE quotes here, or unJSON
        return code

    @staticmethod
    def changePassword(email, oldPassword, newPassword, first_name, last_name):
        realm = settings.CLOUD_CONNECT['password_realm']
        api_url = settings.CLOUD_CONNECT['url']

        passwordString = '%s:%s:%s' % (email, realm, newPassword)
        passwordHA1 = md5(passwordString).hexdigest()

        request = '%s/account/update?%s' % (api_url, django.utils.http.urlencode({
            'passwordHa1':passwordHA1
        }))

        response = requests.get(request, auth=HTTPDigestAuth(email, oldPassword))

        if response.status_code != 200:
            return None

        # REMOVE quotes here, or unJSON
        return response.json()

    @staticmethod
    def update(email, password, first_name, last_name):
        api_url = settings.CLOUD_CONNECT['url']

        full_name = '%s %s' % (first_name, last_name)

        request = '%s/account/update?%s' % (api_url, django.utils.http.urlencode({
            'fullName':full_name
        }))

        response = requests.get(request, auth=HTTPDigestAuth(email, password))

        if response.status_code != 200:
            return None

        # REMOVE quotes here, or unJSON
        return response.json()

    @staticmethod
    def get(email=None, password=None):
        # TODO: create wrappers
        request = settings.CLOUD_CONNECT['url'] + "/account/get"

        response = requests.get(request, auth=HTTPDigestAuth(email, password))

        logger.debug(request)
        logger.debug(response.status_code)
        logger.debug(response.text)
        logger.debug('------------------------------------------------------------')

        return response.json()
