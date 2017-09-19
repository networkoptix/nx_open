from django.conf.urls import url
from django.views.decorators.csrf import csrf_exempt
import views

urlpatterns = [
    url(r'^zap_to_nx',  views.nx_action),
    url(r'^nx_to_zap', views.zap_trigger),
    url(r'ping', views.ping)
]
