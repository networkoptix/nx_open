from rest_framework.response import Response
from rest_framework.decorators import api_view, permission_classes
from rest_framework.permissions import AllowAny, IsAuthenticated
from api.helpers.exceptions import handle_exceptions, APIRequestException, api_success, ErrorCodes, APINotAuthorisedException
from notifications import api
from django.core.exceptions import ValidationError
from django.urls import reverse
from django.conf import settings
from django.contrib import messages

from api.models import Account
from notifications.models import *
from django.utils import timezone
from django.shortcuts import redirect
from django.contrib.auth.decorators import permission_required
import re
import pytz
import datetime

#Replaces </p> and <br> with \n and then remove all html tags
def html_to_text(html):
    new_line = re.compile(r'<(\/p|br)>')
    tags = re.compile(r'<[\w\=\'\"\:\;\_\-\,\!\/\ ]+>')
    return tags.sub('', new_line.sub('\n', html))

def add_brs(html):
    br = re.compile(r'\n')
    return br.sub('<br>', html)


def format_message(notification):
    message = {}
    message['subject'] = notification.subject
    message['html_body'] = add_brs(notification.body)
    message['text_body'] = html_to_text(notification.body)
    return message


#For testing we dont want to send emails to everyone so we need to set
#"BROADCAST_NOTIFICATIONS_SUPERUSERS_ONLY = true" in cloud.settings
def send_to_all_users(sent_by, notification, force=False, tz = None):
    # if forced and not testing dont apply any filters to send to all users
    users = Account.objects.all()

    if not force:
        users = users.filter(subscribe=True)

    if settings.BROADCAST_NOTIFICATIONS_SUPERUSERS_ONLY:
        users = users.filter(is_superuser=True)

    message = format_message(notification)
    
    sent_date = datetime.datetime.now()
    if tz:
        utc = pytz.utc.localize(datetime.datetime.now())
        sent_date = utc.astimezone(pytz.timezone(tz)).replace(tzinfo=None)
    
    notification.sent_by = sent_by
    notification.sent_date = sent_date
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

        user_account = Account.objects.filter(email=request.data['user_email'])
        if user_account.exists():
            request.data['message']['fullName'] = user_account[0].get_full_name()
        api.send(request.data['user_email'],
                 request.data['type'],
                 request.data['message'],
                 request.data['customization'],
                 external_id)
    except ValidationError as error:
        raise APIRequestException(error.message, ErrorCodes.wrong_parameters, error_data=error.detail)
    return api_success()


@api_view(['POST'])
@permission_classes((IsAuthenticated,))
def cloud_notification_action(request):
    notification_id = str(request.data['id'])
    can_add = request.user.has_perm('notifications.add_cloudnotification')
    can_change = request.user.has_perm('notifications.change_cloudnotification')
    can_send = request.user.has_perm('notifications.send_cloud_notification')

    if (can_add and not notification_id or can_change) and 'Save' in request.data:
        notification_id = str(update_or_create_notification(request.data))
        messages.success(request._request, "Changes have been saved")

    elif (can_add and not notification_id or can_change) and 'Preview' in request.data:
        notification_id = str(update_or_create_notification(request.data))
        message = format_message(CloudNotification.objects.get(id=notification_id))
        message['full_name'] = request.user.get_full_name()
        api.send(request.user.email, 'cloud_notification', message, 'default')
        messages.success(request._request, "Preview has been sent")

    elif can_send and 'Send' in request.data and notification_id:
        force = 'ignore_subscriptions' in request.data
        notification = CloudNotification.objects.get(id=notification_id)
        tz = request.session['timezone'] if 'timezone' in request.session else None
        send_to_all_users(request.user, notification, force, tz)
        messages.success(request._request, "Cloud notification has been sent")

    else:
        messages.error(request._request, "Insufficient permission or invalid action")

    if not can_change:
        return redirect('/admin/notifications/')
    return redirect(reverse('admin:notifications_cloudnotification_change', args=[notification_id]))


@api_view(['GET'])
@permission_classes((AllowAny, ))
@handle_exceptions
def test(request):
    from .. import tasks
    from random import seed, randint
    seed()
    tasks.test_task.delay(randint(1,100),randint(1,5))
    return Response('ok')
