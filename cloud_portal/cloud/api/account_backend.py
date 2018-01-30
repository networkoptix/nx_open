from django.utils import timezone
from django import db
import models
from api.controllers.cloud_api import Account
from django.contrib.auth.backends import ModelBackend
from api.helpers.exceptions import APIRequestException, APILogicException, ErrorCodes
from django.core.exceptions import ObjectDoesNotExist
from cloud import settings

import logging
logger = logging.getLogger(__name__)


class AccountBackend(ModelBackend):
    @staticmethod
    def check_email_in_portal(email, check_email_exists):
        mail_exists = models.Account.objects.filter(email=email.lower()).count() > 0
        if not mail_exists and check_email_exists:
            raise APILogicException('User is not in portal', ErrorCodes.not_found)
        if mail_exists and not check_email_exists:
            raise APILogicException('User already registered', ErrorCodes.account_exists)
        return True

    @staticmethod
    def authenticate(username=None, password=None):
        AccountBackend.check_email_in_portal(username, True)
        user = Account.get(username, password)
        if user and 'email' in user:
            try:
                return models.Account.objects.get(email=user['email'])
            except ObjectDoesNotExist:
                raise APILogicException('User is not in portal', ErrorCodes.not_found)
        return None

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
        now = timezone.now()
        if not email:
            raise APIRequestException('Email code is absent', ErrorCodes.wrong_parameters,
                                      error_data={'email': ['This field is required.']})
        email = email.lower()

        logger.debug('AccountManager._create_user called: ' + email)
        # email = self.normalize_email(email)
        first_name = extra_fields.pop("first_name")
        last_name = extra_fields.pop("last_name")

        code = extra_fields.pop("code", None)

        logger.debug('AccountManager._create_user calling /cdb/account/register: ' + email)
        result = Account.register(email, password, first_name, last_name, code=code)
        logger.debug('AccountManager._create_user calling /cdb/account/register - success')

        logger.debug('AccountManager._create_user saving user to cloud_portal: ' + email)
        user = self.model(email=email,
                          first_name=first_name,
                          last_name=last_name,
                          customization=settings.CUSTOMIZATION,
                          **extra_fields)
        user.save(using=self._db)

        logger.debug('AccountManager._create_user completed: ' + email)
        return user

    def create_user(self, email, password, **extra_fields):
        return self._create_user(email, password, **extra_fields)

    def create_superuser(self, email, password, **extra_fields):
        return self._create_user(email, password, **extra_fields)
