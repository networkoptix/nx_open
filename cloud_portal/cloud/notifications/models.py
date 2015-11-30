from django.db import models
from django.utils import timezone
import engines
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
        engines.email.send(self.email, self.type, self.message)
        self.save()
