from django.conf import settings
from django.contrib.auth.models import User


class CloudConnector(object):
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