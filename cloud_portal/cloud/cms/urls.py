from django.conf.urls import include, url
from .views import *

urlpatterns = [
	url(r'context_editor/(?P<context>.+?)/(?P<language>.+?)/', context_edit_view),
	url(r'context_editor/(?P<context>.+?)/', context_edit_view),
	url(r'context_editor/', context_edit_view, name="context_editor"),

	url(r'review_page/(?P<context>.+?)/(?P<language>.+?)/', partner_review_view, name="review_page"),
	url(r'review_page/(?P<context>.+?)/', partner_review_view),
	url(r'review_page/', partner_review_view, name="review_page_no_args"),
	
	url(r'review_version/(?P<id>.+?)/', VersionsViewer.as_view()),
]