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

def get_cloud_modules():
    r = requests.get('http://172.17.0.1:8500/v1/catalog/service/connection_mediator-3345')
    connection_mediator_hosts = [service['ServiceAddress'] for service in r.json() if service['ServiceTags'] is not None and 'udp' in service['ServiceTags']]

    cloud_db_host = os.getenv('CLOUD_DB_HOST', '')
    cloud_portal_host = os.getenv('CLOUD_PORTAL_HOST', '')

    return {
               'cloud_db_host': cloud_db_host,
               'cloud_portal_host': cloud_portal_host,
               'connection_mediator_hosts': connection_mediator_hosts,
           }

@api_view(['GET'])
@permission_classes((AllowAny, ))
def cloud_modules(request):
    return render(request, 'cloud_modules.xml',
                    get_cloud_modules(),
                    content_type='application/xml')

@api_view(['GET'])
@permission_classes((AllowAny, ))
def cloud_modules2(request):
    return render(request, 'cloud_modules2.xml',
                    get_cloud_modules(),
                    content_type='application/xml')
