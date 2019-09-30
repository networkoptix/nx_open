from django.conf.urls import url
from notifications.views.send import cloud_notification_action, test

urlpatterns = [
    url(r'cloud_notification/',
        cloud_notification_action, name="cloud_notification"),
    url(r'^test$', test)
]
