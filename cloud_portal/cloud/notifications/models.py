from django.db import models
from django.utils import timezone
from jsonfield import JSONField


class Message(models.Model):
    user_email = models.CharField(max_length=255)
    task_id = models.CharField(max_length=50, blank=True, editable=False)
    type = models.CharField(max_length=255)
    message = JSONField()
    created_date = models.DateField(auto_now_add=True)
    send_date = models.DateField(null=True, blank=True)

    REQUIRED_FIELDS = ['user_email', 'type', 'message']

    def send(self):
        self.send_date = timezone.now()

        # TODO: initiate business-logic here
        from .tasks import send_email
        result = send_email.delay(self.user_email, self.type, self.message)

        self.task_id = result.task_id
        self.save()

