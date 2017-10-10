"""cloud URL Configuration

The `urlpatterns` list routes URLs to views. For more information please see:
    https://docs.djangoproject.com/en/1.8/topics/http/urls/
Examples:
Function views
    1. Add an import:  from my_app import views
    2. Add a URL to urlpatterns:  url(r'^$', views.home, name='home')
Class-based views
    1. Add an import:  from other_app.views import Home
    2. Add a URL to urlpatterns:  url(r'^$', Home.as_view(), name='home')
Including another URLconf
    1. Add an import:  from blog import urls as blog_urls
    2. Add a URL to urlpatterns:  url(r'^blog/', include(blog_urls))
"""

from django.conf.urls import include, url
from django.contrib import admin
from django.shortcuts import redirect
from django.views.generic.base import TemplateView
from django.conf import settings
from django.conf.urls.static import static

admin.site.disable_action('delete_selected')  # Remove delete action from all models in admin
admin.site.index_template = 'admin/index.html'


def redirect_login(request):
    target_url = '/login'
    if 'QUERY_STRING' in request.META:
        target_url += '?%s' % request.META['QUERY_STRING']
    return redirect(target_url)

urlpatterns = [
    url(r'^admin/login/', redirect_login),
    url(r'^admin/cms/', include('cms.urls')),
    url(r'^admin/', include(admin.site.urls)),
    url(r'^api/', include('api.urls')),
    url(r'^notifications/', include('notifications.urls')),
    url(r'^admin_tools/', include('admin_tools.urls')),
    url(r'^zapier/', include('zapier.urls')),

    url(r'^apple-app-site-association',
        TemplateView.as_view(template_name="static/apple-app-site-association",
                             content_type='application/json')),
    url(r'^\.well-known/apple-app-site-association',
        TemplateView.as_view(template_name="static/apple-app-site-association",
                             content_type='application/json')),
    url(r'^(?!static).*',
        TemplateView.as_view(template_name="static/index.html"))

] + static(settings.STATIC_URL, document_root=settings.STATIC_ROOT)

