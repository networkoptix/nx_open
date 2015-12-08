from rest_framework.decorators import api_view, permission_classes
from rest_framework.permissions import AllowAny, IsAuthenticated
from api.helpers.exceptions import handle_exceptions, APIRequestException, api_success
from notifications import api
from django.core.exceptions import ValidationError

import logging
__django__ = logging.getLogger('django')

@api_view(['POST'])
@permission_classes((AllowAny, ))
@handle_exceptions
def send_notification(request):
    try:
        if 'user_email' not in request.data:
            raise APIRequestException('No user_email in request')
        if 'type' not in request.data:
            raise APIRequestException('No type in request')
        if 'message' not in request.data:
            raise APIRequestException('No message in request')

        api.send(request.data['user_email'], request.data['type'], request.data['message'])
    except ValidationError as error:
        raise APIRequestException(error.message)
    return api_success()

