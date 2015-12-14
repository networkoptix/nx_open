from django.utils import timezone
from django import db
import logging
import models
from api.controllers.cloud_api import Account

from api.helpers.exceptions import APIRequestException, ErrorCodes

__django__ = logging.getLogger('django')


class AccountBackend(object):
    @staticmethod
    def authenticate(username=None, password=None):

        __django__.debug("authentificate " + username)

        checkuser = Account.get(username, password)
        __django__.debug(checkuser)

        if checkuser:
            return models.Account.objects.get(email=username)
        return None

    @staticmethod
    def get_user(user_id):
        try:
            return models.Account.objects.get(pk=user_id)
        except models.Account.DoesNotExist:
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
        if not email or email == '':
            raise APIRequestException('Email code is absent', ErrorCodes.wrong_parameters,
                                      error_data={'email': ['This field is required.']})
        # email = self.normalize_email(email)
        first_name = extra_fields.pop("first_name", True)
        last_name = extra_fields.pop("last_name", True)

        Account.register(email, password, first_name, last_name)

        user = self.model(email=email,
                          first_name=first_name,
                          last_name=last_name,
                          **extra_fields)
        user.save(using=self._db)
        return user

    def create_user(self, email, password, **extra_fields):
        return self._create_user(email, password, **extra_fields)

    def create_superuser(self, email, password, **extra_fields):
        return self._create_user(email, password, **extra_fields)
