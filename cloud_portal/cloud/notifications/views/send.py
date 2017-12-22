from rest_framework.response import Response
from rest_framework.decorators import api_view, permission_classes
from rest_framework.permissions import AllowAny
from api.helpers.exceptions import handle_exceptions, APIRequestException, api_success, ErrorCodes, APINotAuthorisedException
from notifications import api
from django.core.exceptions import ValidationError

from api.models import Account
from notifications.models import *
from datetime import datetime
from django.shortcuts import redirect
from django.contrib.auth.decorators import permission_required
import re

#Replaces </p> and <br> with \n and then remove all html tags
def html_to_text(html):
    new_line = re.compile(r'<(\/p|br)>')
    tags = re.compile(r'<[\/\w]+>')
    return tags.sub('', new_line.sub('\n', html))


def format_message(notification):
    message = {}
    message['subject'] = notification.subject
    message['html_body'] = notification.body
    message['text_body'] = html_to_text(notification.body)
    return message


def send_to_all_users(sent_by, notification):
    users = Account.objects.filter(is_superuser=True)
    message = format_message(notification)
    notification.sent_by = sent_by
    notification.sent_date = datetime.now()
    notification.save()
    for user in users:
        message['full_name'] = user.get_full_name()
        api.send(user.email, 'cloud_notification', message, 'default')


def update_or_create_notification(data):
    if not data['id']:
        notification = CloudNotification(subject=data['subject'], body=data['body'])
    else:
        notification = CloudNotification.objects.get(id=data['id'])
        notification.subject = data['subject']
        notification.body = data['body']
    notification.save()
    return notification.id


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

        api.send(request.data['user_email'],
                 request.data['type'],
                 request.data['message'],
                 request.data['customization'],
                 external_id)
    except ValidationError as error:
        raise APIRequestException(error.message, ErrorCodes.wrong_parameters, error_data=error.detail)
    return api_success()


@api_view(['GET','POST'])
@permission_required('notifications.send_cloud_notification')
def cloud_notification_action(request):
    notification_id = str(request.data['id'])
    if 'Save' in request.data:
        notification_id = str(update_or_create_notification(request.data))
    elif 'Preview' in request.data:
        notification = CloudNotification.objects.get(id=notification_id)
        message = format_message(notification)
        message['full_name'] = request.user.get_full_name()
        api.send(request.user.email, 'cloud_notification', message, 'default')
    elif 'Send' in request.data:
        notification = CloudNotification.objects.get(id=notification_id)
        send_to_all_users(reqeust.user, notification)
    else:
        return Response("Invalid")

    return redirect('/admin/notifications/cloudnotification/' + notification_id + '/change/')


@api_view(['GET'])
@permission_classes((AllowAny, ))
@handle_exceptions
def test(request):
    from .. import tasks
    from random import seed, randint
    seed()
    tasks.test_task.delay(randint(1,100),randint(1,5))
    return Response('ok')
