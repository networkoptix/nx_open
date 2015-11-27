from rest_framework.decorators import api_view, permission_classes
from rest_framework.permissions import AllowAny, IsAuthenticated
from api.controllers import cloud_api

from api.helpers.exceptions import handle_exceptions, api_success
import logging

logger = logging.getLogger('django')


@api_view(['GET'])
@permission_classes((IsAuthenticated, ))
@handle_exceptions
def list_systems(request):
    data = cloud_api.System.list(request.user.email, request.session['password'])
    return api_success(data['systems'])
