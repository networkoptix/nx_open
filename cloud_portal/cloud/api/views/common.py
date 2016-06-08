from rest_framework.decorators import api_view, permission_classes
from rest_framework.permissions import AllowAny, IsAuthenticated
from api.helpers.exceptions import handle_exceptions, api_success
from api.controllers import cloud_api
from django.shortcuts import render
import os
import requests


@api_view(['GET'])
@permission_classes((AllowAny, ))
@handle_exceptions
def ping(request):
    data = cloud_api.ping()
    return api_success(data)


@api_view(['GET'])
@permission_classes((AllowAny, ))
def cloud_modules(request):
    r = requests.get('http://172.17.0.1:8500/v1/catalog/service/connection_mediator-3345')
    connection_mediator_hosts = [service['Address'] for service in r.json()]

    cloud_db_host = os.getenv('CLOUD_DB_HOST', '')
    cloud_portal_host = os.getenv('CLOUD_PORTAL_HOST', '')

    return render(request, 'cloud_modules.xml',
                    {
                        'cloud_db_host': cloud_db_host,
                        'cloud_portal_host': cloud_portal_host,
                        'connection_mediator_hosts': connection_mediator_hosts,
                    },
                    content_type='application/xml')
