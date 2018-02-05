from django.db import models
from django.utils import timezone
from jsonfield import JSONField
from django.conf import settings
from django.db.models import Q
from rest_framework import serializers

#When cloudportal is ran locally it uses amqp by default. BROKER_TRANSPORT_OPTIONS is related to sqs.
#This allows cloud notifications to run locally without changing settings to use sqs.
USE_SQS_FOR_CLOUD_NOTIFICATIONS = hasattr(settings, "BROKER_TRANSPORT_OPTIONS")


class Event(models.Model):
    object = models.CharField(max_length=255)
    type = models.CharField(max_length=255)
    data = JSONField()
    created_date = models.DateTimeField(auto_now_add=True)
    send_date = models.DateTimeField(null=True, blank=True)

    def send(self):
        self.save()
        # 1. Get all subscriptions for this event
        subscriptions = Subscription.objects.filter(Q(type=self.type, object='') |
                                                    Q(type='', object=self.object) |
                                                    Q(type=self.type, object=self.object))

        if settings.NOTIFICATIONS_AUTO_SUBSCRIBE and not subscriptions.exists():
            subscription = Subscription(
                type=self.type,
                user_email=settings.NOTIFICATIONS_AUTO_SUBSCRIBE,
                enabled=True
            )
            subscription.save()
            subscriptions = subscriptions.filter()

        subscriptions = subscriptions.filter(Q(enabled=True) | Q(enabled=1))
        # 2. For each subscription create a message and send it
        for user in subscriptions.all():
            message = Message(
                message=self.data,
                user_email=user.user_email,
                type=self.type,
                event=self
            )
            message.send()


class Subscription(models.Model):
    object = models.CharField(max_length=255, default='', blank=True,
                              help_text="What's the target? (release type, customization or cloud instance)")
    type = models.CharField(max_length=255, default='', blank=True,
                            help_text="What's the event? (submitted_release, published_{{type}}, cloud_...)")
    user_email = models.CharField(max_length=255)
    created_date = models.DateTimeField(auto_now_add=True)
    enabled = models.BooleanField(default=True)


class Message(models.Model):
    user_email = models.CharField(max_length=255)
    external_id = models.CharField(max_length=64, db_index=True, unique=True, blank=True, null=True)
    task_id = models.CharField(max_length=50, blank=True, editable=False)
    type = models.CharField(max_length=255)
    customization = models.CharField(max_length=255, default='default')
    message = JSONField()
    created_date = models.DateTimeField(auto_now_add=True)
    send_date = models.DateTimeField(null=True, blank=True)
    event = models.ForeignKey(Event, null=True)

    REQUIRED_FIELDS = ['user_email', 'type', 'message']

    def send(self):
        self.send_date = timezone.now()

        # TODO: initiate business-logic here
        from .tasks import send_email

        if settings.USE_ASYNC_QUEUE:
            if USE_SQS_FOR_CLOUD_NOTIFICATIONS and 'queue' in settings.NOTIFICATIONS_CONFIG[self.type]:
                result = send_email.apply_async(args=[self.user_email, self.type, self.message, self.customization],
                                                queue=settings.NOTIFICATIONS_CONFIG[self.type]['queue'])
            else:
                result = send_email.delay(self.user_email, self.type, self.message, self.customization)
            self.task_id = result.task_id
        else:
            send_email(self.user_email, self.type, self.message, self.customization)
            self.task_id = 'sync'

        self.save()


    def delivery_time_interval(self):
        return (self.created_date - self.send_date).total_seconds()

    delivery_time_interval.short_description = "Delivery Time Interval (sec)"


class MessageStatusSerializer(serializers.ModelSerializer):  # model to use when checking on message status
    class Meta:
        model = Message
        fields = ('external_id', 'task_id', 'type', 'customization', 'created_date', 'send')


class CloudNotification(models.Model):
    class Meta:
        permissions = (
            ("send_cloud_notification", "Can send cloud notifications"),
        )

    subject = models.CharField(max_length=255)
    body = models.TextField()
    sent_date = models.DateTimeField(null=True, blank=True)
    sent_by = models.ForeignKey(
        settings.AUTH_USER_MODEL, null=True, blank=True,
        related_name='accepted_%(class)s')