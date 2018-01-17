from django.conf.urls import url
from views.send import send_notification, cloud_notification_action
from views.send import test

urlpatterns = [
    url(r'cloud_notification/',
            cloud_notification_action, name="cloud_notification"),
    url(r'^send$',  send_notification),
    url(r'^test$', test)
]
