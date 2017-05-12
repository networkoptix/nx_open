from django.conf.urls import url
from views.send import send_notification
from views.send import test

urlpatterns = [
    url(r'^send$',  send_notification),
    url(r'^test$', test)
]
