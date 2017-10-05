from django.conf.urls import url
import views

urlpatterns = [
    url(r'^ping', views.ping),
    url(r'^send_generic_event',  views.zapier_send_generic_event),
    url(r'^get_systems', views.get_systems),
    url(r'^trigger_zapier', views.nx_http_action),
    url(r'^subscribe', views.create_zap_webhook),
    url(r'^unsubscribe', views.remove_zap_webhook),
    url(r'^poll_for_subscribe', views.test_subscribe),
]
