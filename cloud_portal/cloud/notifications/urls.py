from django.conf.urls import url
from views.send import send_event, send_notification

urlpatterns = [
    url(r'^send$',  send_notification),
    url(r'^feedback', send_event)
]
