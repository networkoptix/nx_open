from rest_framework import status
from rest_framework.decorators import api_view, permission_classes
from rest_framework.response import Response
from account_models import AccountSerializer, CreateAccountSerializer
from rest_framework.permissions import AllowAny, IsAuthenticated
from api.controllers import cloud_api
import django
import json
import logging

logger = logging.getLogger('django')


@api_view(['GET'])
@permission_classes((IsAuthenticated, ))
def list(request):

    logger.debug("systems-list")

    data = cloud_api.system.list(request.user.email, request.session['password'])

    logger.debug(json.dumps(data))

    return Response(data['systems'], status=200)
