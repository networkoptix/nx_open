from django.db import models
from django.utils import timezone
from engines import email
from jsonfield import JSONField


class Message(models.Model):
    user_email = models.CharField(max_length=255)
    type = models.CharField(max_length=255)
    message = JSONField()
    created_date = models.DateField(auto_now_add=True)
    send_date = models.DateField(null=True, blank=True)

    REQUIRED_FIELDS = ['user_email', 'type', 'message']

    def send(self):
        self.send_date = timezone.now()

        # TODO: initiate business-logic here
        email.send(self.user_email, self.type, self.message)
        self.save()
