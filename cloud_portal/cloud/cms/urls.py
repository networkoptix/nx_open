from django.conf.urls import url
from cms.views import integration


urlpatterns = [
    url(r'^integration/(?P<product_id>.+?)$', integration.get_integration, name="get_integration"),
    url(r'^integrations$', integration.get_integrations, name="get_integrations"),
]
