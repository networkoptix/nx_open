from django.conf.urls import include, url
from .views import *

urlpatterns = [
	url(r'', page_edit_view),
]