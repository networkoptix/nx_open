from rest_framework.decorators import api_view, permission_classes
from rest_framework.permissions import AllowAny
from api.helpers.exceptions import handle_exceptions, APIRequestException, api_success, ErrorCodes
from notifications.helpers.ipwhitelists import ip_allow_only
from notifications import api
from django.core.exceptions import ValidationError


@api_view(['POST'])
@permission_classes((AllowAny, ))
@handle_exceptions
@ip_allow_only('local')
def send_notification(request):
    try:
        validation_error = False
        error_data = {}
        if 'user_email' not in request.data or request.data['user_email'] == '':
            validation_error = True
            error_data['user_email'] = ['This field is required.']

        if 'type' not in request.data or request.data['type'] == '':
            validation_error = True
            error_data['type'] = ['This field is required.']
        if 'message' not in request.data:
            validation_error = True
            error_data['message'] = ['This field is required.']

        if validation_error:
            raise APIRequestException('Not enough parameters in request', ErrorCodes.wrong_parameters,
                                      error_data=error_data)

        api.send(request.data['user_email'], request.data['type'], request.data['message'])
    except ValidationError as error:
        raise APIRequestException(error.message, ErrorCodes.wrong_parameters, error_data=error.detail)
    return api_success()

