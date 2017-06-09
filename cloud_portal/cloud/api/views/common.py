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
    connection_mediator_port = os.environ['CONNECTION_MEDIATOR_PORT']
    r = requests.get('http://consul:8500/v1/catalog/service/connection_mediator-{}'.format(connection_mediator_port))
    connection_mediator_hosts = [service['ServiceAddress'] for service in r.json() if service['ServiceTags'] is not None and 'udp' in service['ServiceTags']]

    cloud_portal_host = os.environ['CLOUD_PORTAL_HOST']
    cloud_portal_port = os.environ['CLOUD_PORTAL_PORT']
    cloud_portal_sport = os.environ['CLOUD_PORTAL_SPORT']
    cloud_db_host = os.environ['CLOUD_DB_HOST']
    cloud_db_port = os.environ['CLOUD_DB_PORT']
    cloud_db_sport = os.environ['CLOUD_DB_SPORT']
    vms_gateway_host = os.environ['VMS_GATEWAY_HOST']
    vms_gateway_port = os.environ['VMS_GATEWAY_PORT']

    return {
               'cloud_db_host': cloud_db_host,
               'cloud_db_port': cloud_db_port,
               'cloud_db_sport': cloud_db_sport,
               'cloud_portal_host': cloud_portal_host,
               'cloud_portal_port': cloud_portal_port,
               'cloud_portal_sport': cloud_portal_sport,
               'vms_gateway_host': vms_gateway_host,
               'vms_gateway_port': vms_gateway_port,
               'connection_mediator_hosts': connection_mediator_hosts,
               'connection_mediator_port': connection_mediator_port,
           }


@api_view(['GET'])
@permission_classes((AllowAny, ))
def cloud_modules2(request):
    return render(request, 'cloud_modules2.xml',
                    get_cloud_modules(),
                    content_type='application/xml')


@api_view(['GET'])
@permission_classes((AllowAny, ))
def cloud_modules_json(request):
    return api_success(get_cloud_modules())

