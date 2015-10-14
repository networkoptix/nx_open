from django.db import models
from account import AccountManager

class Account(models.Model):

    objects = AccountManager()

    email = models.CharField(unique=True,max_length = 255)
    first_name = models.CharField(max_length = 255)
    last_name = models.CharField(max_length = 255)
    created_date = models.DateField()
    activated_date = models.DateField()
    last_login = models.DateField()

    USERNAME_FIELD = 'email'
    REQUIRED_FIELDS = ['registeredDate', 'createdDate']

    def get_full_name(self):
        return self.email

    def get_short_name(self):
        return self.first_name

    def is_authenticated(self):
        return True


