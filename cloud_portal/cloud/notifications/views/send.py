from rest_framework.response import Response
from rest_framework.decorators import api_view, permission_classes
from rest_framework.permissions import AllowAny, IsAuthenticated

from django.core.exceptions import PermissionDenied, ValidationError
from django.urls import reverse
from django.conf import settings
from django.utils import timezone
from django.contrib import messages
from django.shortcuts import redirect

from api.helpers.exceptions import handle_exceptions, APIRequestException, api_success, ErrorCodes, APINotAuthorisedException
from api.models import Account
from cms.models import Customization, UserGroupsToCustomizationPermissions
from notifications import api
from notifications.models import *
from notifications.tasks import send_to_all_users

import re

# Replaces </p> and <br> with \n and then remove all html tags
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


def update_or_create_notification(data, customizations=[]):
    if not data['id']:
        notification = CloudNotification(subject=data['subject'], body=data['body'])
    else:
        notification = CloudNotification.objects.get(id=data['id'])
        notification.subject = data['subject']
        notification.body = data['body']
        notification.save()
        customization_ids = list(Customization.objects.filter(name__in=customizations).values_list('id', flat=True))
        notification.customizations = customization_ids
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

        # Clouddb doesn't always return a full name so try to get it from cloud portal
        if 'userFullName' not in request.data['message'] or not request.data['message']['userFullName']:
            user_account = Account.objects.filter(email=request.data['user_email'])
            if user_account.exists():
                request.data['message']['userFullName'] = user_account[0].get_full_name()

        api.send(request.data['user_email'],
                 request.data['type'],
                 request.data['message'],
                 request.data['customization'],
                 external_id)
    except ValidationError as error:
        error_data = error.detail if hasattr(error, 'detail') else None
        raise APIRequestException(error.message, ErrorCodes.wrong_parameters, error_data=error_data)
    return api_success()


# Refactor later add state for messages, enforce review before allowing to send
@api_view(['POST'])
@permission_classes((IsAuthenticated,))
def cloud_notification_action(request):
    can_add = request.user.has_perm('notifications.add_cloudnotification')
    can_change = request.user.has_perm('notifications.change_cloudnotification')
    can_send = request.user.has_perm('notifications.send_cloud_notification')

    customizations = [settings.CUSTOMIZATION]
    if 'customizations' in request.data:
        customizations = request.data.getlist('customizations')

    for customization in customizations:
        if not UserGroupsToCustomizationPermissions.check_permission(request.user, customization,
                                                                     'send_cloud_notification'):
            raise PermissionDenied

    notification_id = str(update_or_create_notification(request.data, customizations))

    if (can_add and not notification_id or can_change) and 'Save' in request.data:
        messages.success(request._request, "Changes have been saved")

    elif (can_add and not notification_id or can_change) and 'Preview' in request.data:
        message = format_message(CloudNotification.objects.get(id=notification_id))
        message['userFullName'] = request.user.get_full_name()

        api.send(request.user.email, 'cloud_notification', message, request.user.customization)
        messages.success(request._request, "Preview has been sent")

    elif can_send and 'Send' in request.data and notification_id:
        force = 'ignore_subscriptions' in request.data

        notification = CloudNotification.objects.get(id=notification_id)
        message = format_message(notification)

        notification.sent_by = request.user
        notification.sent_date = timezone.now()
        notification.save()
        send_to_all_users.apply_async(args=[notification_id, message, customizations, force],
                                      queue=settings.NOTIFICATIONS_CONFIG['cloud_notification']['queue'])
        messages.success(request._request, "Sending cloud notifications")

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
