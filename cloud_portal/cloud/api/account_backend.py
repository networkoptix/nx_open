import logging

from django import db
from django.utils import timezone
from django.contrib.auth.backends import ModelBackend
from django.core.exceptions import ObjectDoesNotExist
from django.contrib.auth.signals import user_logged_in, user_logged_out, user_login_failed
from django.dispatch import receiver

from cloud import settings

import models
from api.controllers.cloud_api import Account
from api.helpers.exceptions import APIRequestException, APIException, APILogicException, ErrorCodes, APINotAuthorisedException


logger = logging.getLogger(__name__)


class AccountBackend(ModelBackend):
    @staticmethod
    def is_email_in_portal(email):
        return models.Account.objects.filter(email=email.lower()).count() > 0

    @staticmethod
    def check_email_in_portal(email, check_email_exists):
        mail_exists = AccountBackend.is_email_in_portal(email)
        if not mail_exists and check_email_exists:
            raise APILogicException('User is not in portal', ErrorCodes.not_found)
        if mail_exists and not check_email_exists:
            raise APILogicException('User already registered', ErrorCodes.account_exists)
        return True

    @staticmethod
    def authenticate(request=None, username=None, password=None):
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

            if not AccountBackend.is_email_in_portal(user['email']):
                # so - user is in cloud_db, but not in cloud_portal
                raise APILogicException('User is not in portal', ErrorCodes.portal_critical_error)
        return models.Account.objects.get(email=user['email'])

    @staticmethod
    def get_user(user_id):
        try:
            return models.Account.objects.get(pk=user_id)
        except ObjectDoesNotExist:
            return None


class AccountManager(db.models.Manager):

    """Custom manager for Account."""
    def _create_user(self, email, password, **extra_fields):
        """Create and save an Account with the given email and password.
        :param str email: user email
        :param str password: user password
        :param bool is_staff: whether user staff or not
        :param bool is_superuser: whether user admin or not
        :return custom_user.models.Account user: user
        :raise ValueError: email is not set
        """
        if not email:
            raise APIRequestException('Email code is absent', ErrorCodes.wrong_parameters,
                                      error_data={'email': ['This field is required.']})
        email = email.lower()

        ip = extra_fields.pop("IP", "")
        first_name = extra_fields.pop("first_name")
        last_name = extra_fields.pop("last_name")
        code = extra_fields.pop("code", None)

        # this line will send request to cloud_db and raise an exception if fails:
        try:
            Account.register(ip, email, password, first_name, last_name, code=code)
        except APIException as a:
            if a.error_code == ErrorCodes.account_exists and not AccountBackend.is_email_in_portal(email):
                raise APILogicException('User is not in portal', ErrorCodes.portal_critical_error)
            else:
                raise
        user = self.model(email=email,
                          first_name=first_name,
                          last_name=last_name,
                          customization=settings.CUSTOMIZATION,
                          **extra_fields)

        if code:
            user.activated_date = timezone.now()
        user.save(using=self._db)

        return user

    def create_user(self, email, password, **extra_fields):
        return self._create_user(email, password, **extra_fields)

    def create_superuser(self, email, password, **extra_fields):
        return self._create_user(email, password, **extra_fields)

    def register_cloud_invite_user(self, email, password, data):
        ip = data.pop("IP", "")
        first_name = data.pop("first_name")
        last_name = data.pop("last_name")

        Account.register(ip, email, password, first_name, last_name)
        user = models.Account.objects.get(email=email)
        """
        When an account is created using cloud invites it is disabled because its registration
        is different from regular users. Once the user has registered their account it is set to
        active in this function.
        """
        user.is_active = True
        user.first_name = first_name
        user.last_name = last_name
        user.save()

        return user




def get_ip(request):
    return request.META.get('HTTP_X_FORWARDED_FOR')


@receiver(user_logged_in)
def user_logged_in_callback(sender, request, user, **kwargs):
    ip = get_ip(request)
    logger.info('User logged in: {}, IP: {}'.format(user.email, ip))
    models.AccountLoginHistory.objects.create(action='user_logged_in', ip=ip, email=user.email)


@receiver(user_logged_out)
def user_logged_out_callback(sender, request, user, **kwargs):
    ip = get_ip(request)
    logger.info('User logged our: {}, IP: {}'.format(user.email, ip))
    models.AccountLoginHistory.objects.create(action='user_logged_out', ip=ip, email=user.email)


@receiver(user_login_failed)
def user_login_failed_callback(sender, credentials, **kwargs):
    logger.info('Failed login attempt: {}'.format(credentials.get('username', None)))
    models.AccountLoginHistory.objects.create(action='user_login_failed', email=credentials.get('username', None))
