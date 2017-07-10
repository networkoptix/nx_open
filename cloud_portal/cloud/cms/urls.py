from django.conf.urls import include, url
from .views import *

urlpatterns = [
	url(r'context_editor/(?P<context>.+?)/(?P<language>.+?)/', context_edit_view, name="context_editor"),
	url(r'context_editor/(?P<context>.+?)/', context_edit_view, name="context_editor"),

	url(r'review_version_request/', review_version_request, name="review_page_actions"),
	
	url(r'review_version/(?P<version_id>.+?)/', review_version_view, name="review_version"),
]