from django.conf.urls import url
from cms.views import product

urlpatterns = [
    url(r'download/(?P<path>.*)$', product.download_file, name="download_file"),

    url(r'edit/(?P<context_id>.+?)/(?P<language_code>.+?)/(?P<product_id>.+?)/', product.page_editor, name="page_editor"),

    url(r'edit/(?P<context_id>.+?)/(?P<language_code>.+?)/', product.page_editor, name="page_editor"),

    url(r'edit/(?P<context_id>.+?)/', product.page_editor, name="page_editor"),

    url(r'preview/', product.make_preview, name="preview"),

    url(r'package/(?P<product_id>.*$)', product.download_package, name="download_package"),

    url(r'product_settings/(?P<product_id>.+?)/', product.product_settings, name="product_settings"),

]
