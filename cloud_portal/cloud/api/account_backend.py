import logging

from django.contrib.auth.backends import ModelBackend
from django.core.exceptions import ObjectDoesNotExist
from django.contrib.auth.signals import user_logged_in, user_logged_out, user_login_failed
from django.dispatch import receiver

from api.models import *
from api.helpers.exceptions import APILogicException, ErrorCodes, APINotAuthorisedException


logger = logging.getLogger(__name__)

IP_MAX_LENGTH = 255


def get_ip(request):
    ip = request.META.get('HTTP_X_FORWARDED_FOR')
    return ip if len(ip) <= IP_MAX_LENGTH else ip[:IP_MAX_LENGTH]


class AccountBackend(ModelBackend):
    def authenticate(self, request=None, username=None, password=None):
        try:
            ip = get_ip(request)
            user = Account.get(username, password, ip)  # first - check cloud_db
        except APINotAuthorisedException as exception:
            if request and exception.error_code == ErrorCodes.account_blocked:
                request.session['account_blocked'] = True
            return None  # not authorised - return None which tells django that auth failed and it will log it

        if user and 'email' in user:
            if username.find('@') > -1:
                if username != user['email']:  # code and email from cloud_db are wrong
                    raise APILogicException('Login does not match users email', ErrorCodes.wrong_code)
            elif username.find('-') > -1:  # CLOUD-1661 - temp login now has format: guid-crc32(accountEmail)
                import zlib
                (uuid, temp_crc32) = username.split('-')
                email_crc32 = zlib.crc32(user['email']) & 0xffffffff  # convert signed to unsigned crc32
                if email_crc32 != int(temp_crc32):
                    raise APILogicException('Login does not match users email', ErrorCodes.wrong_code)

            if not AccountManager.is_email_in_portal(user['email']):
                # so - user is in cloud_db, but not in cloud_portal
                raise APILogicException('User is not in portal', ErrorCodes.portal_critical_error)
        return Account.objects.get(email=user['email'])

    def get_user(self, user_id):
        try:
            return Account.objects.get(pk=user_id)
        except ObjectDoesNotExist:
            return None


@receiver(user_logged_in)
def user_logged_in_callback(sender, request, user, **kwargs):
    ip = get_ip(request)
    logger.info('User logged in: {}, IP: {}'.format(user.email, ip))
    AccountLoginHistory.objects.create(action='user_logged_in', ip=ip, email=user.email)


@receiver(user_logged_out)
def user_logged_out_callback(sender, request, user, **kwargs):
    ip = get_ip(request)
    logger.info('User logged out: {}, IP: {}'.format(user.email, ip))
    AccountLoginHistory.objects.create(action='user_logged_out', ip=ip, email=user.email)


@receiver(user_login_failed)
def user_login_failed_callback(sender, credentials, **kwargs):
    logger.info('Failed login attempt: {}'.format(credentials.get('username', None)))
    AccountLoginHistory.objects.create(action='user_login_failed', email=credentials.get('username', None))
