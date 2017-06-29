from django.conf.urls import include, url
from .views import *

urlpatterns = [
	url(r'edit_page/', page_edit_view),
	url(r'review_versions/', VersionsViewer.as_view()),
	url(r'review_page/',partner_publish_view),
]