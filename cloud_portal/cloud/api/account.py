
from django.utils import timezone

from django import db
import logging
import models
logger = logging.getLogger('django')

from api.controllers.cloud_api import account

class AccountBackend(object):
    def authenticate(self, username=None, password=None):

        logger.debug("authentificate " + username)

        checkUser = account.get(username, password)
        logger.debug(checkUser)

        if checkUser:
            return models.Account.objects.get(email=username)
        return None

    def get_user(self, user_id):
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
        if not email:
            raise ValueError('The given email must be set')
        #email = self.normalize_email(email)
        first_name = extra_fields.pop("first_name", True)
        last_name = extra_fields.pop("last_name", True)
        created_date = now

        if account.register(email, password, first_name, last_name) == None :
            return None

        user = self.model(email=email,
                          first_name=first_name,
                          last_name=last_name,
                          created_date=created_date,
                          **extra_fields)
        user.save(using=self._db)
        return user

    def create_user(self, email, password, **extra_fields):
        return self._create_user(email, password, **extra_fields)

    def create_superuser(self, email, password, **extra_fields):
        return self._create_user(email, password, **extra_fields)
