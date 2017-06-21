from django.conf.urls import include, url
from .views import *

urlpatterns = [
	url(r'', model_list_view),
]