from django.conf.urls import url
import views

urlpatterns = [
    url(r'^zap_to_nx',  views.nx_action),
    url(r'^list_systems', views.list_systems),
    url(r'^fire_hook', views.fire_zap_webhook),
    url(r'^subscribe', views.create_zap_webhook),
    url(r'^unsubscribe', views.remove_zap_webhook),
    url(r'ping', views.ping),
]
