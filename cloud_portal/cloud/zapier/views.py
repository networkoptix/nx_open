import django
from rest_framework.decorators import api_view, permission_classes
from rest_framework.permissions import AllowAny
from rest_framework.response import Response

from api.helpers.exceptions import handle_exceptions, api_success, APINotAuthorisedException
from api.controllers import cloud_api

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
'''


@api_view(['GET'])
@permission_classes((AllowAny, ))
@handle_exceptions
def get_systems(request):
    user, email, password = authenticate(request)
    data = cloud_api.System.list(email, password)
    zap_list = {'systems': []}

    for i in range(data['systems']):
        zap_list['systems'].append({'id': i, 'system_id': data['systems'][i]})

    return api_success(zap_list)
    #format needs to be {'systems': [{'id': '1', 'system_id': 'a'}, {'id': '2', 'system_id': 'b'}]}

@api_view(['POST'])
@permission_classes((AllowAny, ))
@handle_exceptions
def nx_action(request):
    user, email, password = authenticate(request)

    instance = "https://cloud-test.hdw.mx/gateway/"
    system_id = request.data['systemId']
    source = request.data['source']
    caption = request.data['caption']
    description = "&description=" + request.data['description'] if 'description' in request.data else ""

    url = "{}{}/api/createEvent?source={}&caption={}{}"\
        .format(instance, system_id, source, caption, description)

    headers = {'Content-Type': 'application/json'}
    r = requests.get(url, data=None, headers=headers, auth=HTTPDigestAuth(email, password))

    return json.loads(r.text)


@api_view(['GET', 'POST'])
@permission_classes((AllowAny, ))
@handle_exceptions
def fire_zap_webhook(request):
    caption = request.query_params['caption']
    event = request.query_params['system_id'] + ' ' + caption
    hooks_event = Hook.objects.filter(event=event)

    if hooks_event.exists():
        for hook in hooks_event:
            raw_hook_event.send(sender=None, event_name=event, payload={'data': {'caption': caption}}, user=hook.user)

        return Response({'message': "webhook fired for " + caption}, status=200)

    else:
        return Response({'message': "webhook for " + caption + " does not exist"}, status=404)


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
    system_id = request.query_params['system_id']
    caption = request.query_params['caption']
    target = request.data['target_url']
    event = system_id + " " + caption

    user_hooks = Hook.objects.filter(user=user, target=target)
    if user_hooks.exists():
        return Response({'message': 'There is already a webhook for ' + caption, 'link': None}, status=500)

    url_link = '/firehook/?system_id=%s&caption=%s' %(system_id, target)
    zap_hook = Hook(user=user, event=event, target=target)
    zap_hook.save()
    return Response({'message': 'Webhook created for ' + caption, 'link': url_link}, status=200)


@api_view(['POST'])
@permission_classes((AllowAny, ))
@handle_exceptions
def remove_zap_webhook(request):
    user, email, password = authenticate(request)
    target = request.data['target_url']

    user_hooks = Hook.objects.filter(user=user, target=target)
    if not user_hooks.exists():
        return Response({'message': "Webhook for " + target + " does not exist"}, status=500)
    event = user_hooks[0].event
    user_hooks.delete()
    return Response({'message': 'Webhook deleted for ' + event}, status=200)