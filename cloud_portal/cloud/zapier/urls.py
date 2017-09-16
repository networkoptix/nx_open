from django.conf.urls import url
import views

urlpatterns = [
    url(r'get_systems', views.list_systems),
    url(r'^zap_to_nx',  views.nx_action),
    url(r'^nx_to_zap', views.zap_trigger),
    url(r'ping', views.ping)
]
