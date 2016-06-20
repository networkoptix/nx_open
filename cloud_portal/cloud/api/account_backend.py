from django.utils import timezone
from django import db
import models
from api.controllers.cloud_api import Account
from api.helpers.exceptions import APIRequestException, APILogicException, ErrorCodes
from django.core.exceptions import ObjectDoesNotExist


class AccountBackend(object):
    @staticmethod
    def authenticate(username=None, password=None):
        user = Account.get(username, password)
        if user and 'email' in user:
            try:
                return models.Account.objects.get(email=user['email'])
            except ObjectDoesNotExist:
                raise APILogicException('User is not in portal', ErrorCodes.portal_critical_error)

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
