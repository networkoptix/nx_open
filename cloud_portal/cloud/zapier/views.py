import django
from rest_framework.decorators import api_view, permission_classes
from rest_framework.permissions import AllowAny
from rest_framework.response import Response
from api.helpers.exceptions import handle_exceptions, APINotAuthorisedException

import requests, json, base64
from requests.auth import HTTPDigestAuth
from django.http import JsonResponse

from rest_hooks.models import Hook
from rest_hooks.signals import raw_hook_event


def authenticate(request):
    user, email, password = None, None, None

    if "HTTP_AUTHORIZATION" in request.META:
        credentials = request.META['HTTP_AUTHORIZATION'].split()
        if credentials[0].lower() == "basic":
            email, password = base64.b64decode(credentials[1]).split(':', 1)
            user = django.contrib.auth.authenticate(username=email, password=password)

    if user is None:
        raise APINotAuthorisedException('Username or password are invalid')

    return user, email, password


'''
whole url
https://cloud-test.hdw.mx/gateway/9aeb4e34-d495-4e93-8ae5-3af9630ffc0e/ec2/getUsers

instance
https://cloud-test.hdw.mx/gateway/

system
9aeb4e34-d495-4e93-8ae5-3af9630ffc0e

apicall
/ec2/getUsers

to send requests to cloud test
return requests.post(request, json=params, auth=HTTPDigestAuth(email, password))
'''

@api_view(['POST'])
@permission_classes((AllowAny, ))
@handle_exceptions
def nx_action(request):
    user, email, password = authenticate(request)

    instance = "https://cloud-test.hdw.mx/gateway/"
    system_id = request.data['systemId']

    source = 'source=' + request.data['source']
    caption = '&caption=' + request.data['caption']
    state = '&state=' + request.data['state']

    url = "%s%s%s%s%s%s" % (instance, system_id, '/api/createEvent?', source, caption, state)

    headers = {'Content-Type': 'application/json'}
    r = requests.get(url, data=None, headers=headers, auth=HTTPDigestAuth(email, password))

    return json.loads(r.text)


@api_view(['POST'])
@permission_classes((AllowAny, ))
@handle_exceptions
def fire_zap_webhook(request):
    user, email, password = authenticate(request)
    event = request.data['event']
    raw_hook_event.send(sender=None, event_name=event, payload={'data': request.data}, user=user)
    return Response({'message': "webhook fired for " + event}, status=200)


@api_view(['GET', 'POST'])
@permission_classes((AllowAny, ))
def ping(request):
    authenticate(request)
    return JsonResponse({'status': 'ok'})


@api_view(['POST'])
@permission_classes((AllowAny, ))
@handle_exceptions
def create_zap_webhook(request):
    user, email, password = authenticate(request)
    event = request.data['event']
    target = request.data['target_url']

    user_hooks = Hook.objects.filter(user=user, target=target)
    if user_hooks.exists():
        return Response({'message': 'There is already a webhook for ' + event}, status=500)

    zap_hook = Hook(user=user, event=event, target=target)
    zap_hook.save()
    return Response({'message': 'Webhook created for ' + event}, status=200)


@api_view(['POST'])
@permission_classes((AllowAny, ))
@handle_exceptions
def remove_zap_webhook(request):
    user, email, password = authenticate(request)
    target = request.data['target_url']

    user_hooks = Hook.objects.filter(user=user, target=target)
    if not user_hooks.exists():
        return Response({'message': "Webhook for " + event + " does not exist"}, status=500)
    user_hooks.delete()
    return Response({'message': 'Webhook deleted for ' + event}, status=200)