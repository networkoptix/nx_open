
from django.utils import timezone
from django.db import models

class AccountBackend(object):
    def authenticate(self, username=None, password=None):
        checkUser = True # call cloud here

        if checkUser:
            return User.objects.get(username=username)
        return None

    def get_user(self, user_id):
        try:
            return User.objects.get(pk=user_id)
        except User.DoesNotExist:
            return None

class AccountManager(models.Manager):

    """Custom manager for Account."""
    def _create_user(self, email, **extra_fields):
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

        user = self.model(email=email,
                          first_name=first_name,
                          last_name=last_name,
                          created_date=created_date,
                          **extra_fields)
        user.save(using=self._db)
        return user

    def create_user(self, email, **extra_fields):
        return self._create_user(email, **extra_fields)

    def create_superuser(self, email, **extra_fields):
        return self._create_user(email, **extra_fields)
