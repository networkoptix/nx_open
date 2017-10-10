from django.conf.urls import url
import views

urlpatterns = [
    url(r'^ping', views.ping),
    url(r'^get_systems', views.get_systems),
    url(r'^subscribe', views.subscribe_webhook),
    url(r'^unsubscribe', views.unsubscribe_webhook),
    url(r'^poll_for_subscribe', views.test_subscribe),
    url(r'^send_generic_event',  views.zapier_send_generic_event),
    url(r'.*', views.nx_http_action),
]
