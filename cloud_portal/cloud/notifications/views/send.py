from rest_framework.decorators import api_view, permission_classes
from rest_framework.permissions import AllowAny
from api.helpers.exceptions import handle_exceptions, APIRequestException, api_success, ErrorCodes, APINotAuthorisedException
from api.models import Account
from notifications import api, models
from django.core.exceptions import ValidationError


@api_view(['POST'])
@permission_classes((AllowAny, ))
@handle_exceptions
def send_notification(request):
    try:
        validation_error = False
        error_data = {}

        external_id = None
        if 'id' in request.data:  # external service generated an id for this message to track it
            external_id = request.data['id']
            msg = api.find_message(external_id)
            if msg:
                # there is already a message with this id - do not send the message, respond with status
                serializer = models.MessageStatusSerializer(msg, many=False)
                return api_success(serializer.data)

        if 'user_email' not in request.data or not request.data['user_email']:
            validation_error = True
            error_data['user_email'] = ['This field is required.']

        if 'type' not in request.data or not request.data['type']:
            validation_error = True
            error_data['type'] = ['This field is required.']
        if 'message' not in request.data:
            validation_error = True
            error_data['message'] = ['This field is required.']

        if 'customization' not in request.data or not request.data['customization']:
            validation_error = True
            error_data['customization'] = ['This field is required.']

        if validation_error:
            raise APIRequestException('Not enough parameters in request', ErrorCodes.wrong_parameters,
                                      error_data=error_data)

        user_account = Account.objects.get(email=request.data['user_email'])
        request.data['message']['fullName'] = user_account.get_full_name()
        api.send(request.data['user_email'],
                 request.data['type'],
                 request.data['message'],
                 request.data['customization'],
                 external_id)
    except ValidationError as error:
        raise APIRequestException(error.message, ErrorCodes.wrong_parameters, error_data=error.detail)
    return api_success()


@api_view(['GET'])
@permission_classes((AllowAny, ))
@handle_exceptions
def test(request):
    from rest_framework.response import Response
    from .. import tasks
    from random import seed, randint
    seed()
    tasks.test_task.delay(randint(1,100),randint(1,5))
    return Response('ok')
