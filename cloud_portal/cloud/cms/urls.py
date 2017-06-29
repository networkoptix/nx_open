from django.conf.urls import include, url
from .views import *

urlpatterns = [
	url(r'context_editor/', context_edit_view),
	url(r'review_version/', VersionsViewer.as_view()),
	url(r'review_page/',partner_publish_view),
]