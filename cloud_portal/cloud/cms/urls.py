from django.conf.urls import include, url
from .views import *

urlpatterns = [
    url(r'edit/(?P<context_id>.+?)/(?P<language_code>.+?)/', page_editor, name="page_editor"),
    url(r'edit/(?P<context_id>.+?)/', page_editor, name="page_editor"),

    url(r'download/(?P<path>.*)$', download_file, name="download_file"),

    url(r'package/(?P<product_name>.*$)', download_package, name="download_package"),
    url(r'package/(?P<product_name>.*?)/(?P<customization_name>.*)$', download_package, name="download_package"),

    url(r'product_settings/(?P<product_id>.+?)/', product_settings, name="product_settings"),

    url(r'version_action/(?P<version_id>.+?)/', version_action, name="version_action"),

    url(r'version/(?P<version_id>.+?)/', version, name="version"),
]
