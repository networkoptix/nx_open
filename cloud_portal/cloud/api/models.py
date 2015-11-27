from django.db import models
from account_backend import AccountManager


class Account(models.Model):

    objects = AccountManager()

    email = models.CharField(unique=True, max_length=255)
    first_name = models.CharField(max_length=255)
    last_name = models.CharField(max_length=255)
    created_date = models.DateField()
    activated_date = models.DateField(null=True, blank=True)
    last_login = models.DateField(null=True, blank=True)
    subscribe = models.BooleanField(default=False)

    USERNAME_FIELD = 'email'
    REQUIRED_FIELDS = ['registeredDate', 'createdDate']

    def get_full_name(self):
        return self.email

    def get_short_name(self):
        return self.first_name

    @staticmethod
    def is_authenticated():
        return True
