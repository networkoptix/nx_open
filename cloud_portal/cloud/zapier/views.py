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

from models import *


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

    for i in range(len(data['systems'])):
        zap_list['systems'].append({'name': data['systems'][i]['name'], 'system_id': data['systems'][i]['id']})

    return api_success(zap_list)
    #format needs to be {'systems': [{'id': '1', 'system_id': 'a'}, {'id': '2', 'system_id': 'b'}]}

@api_view(['POST'])
@permission_classes((AllowAny, ))
@handle_exceptions
def nx_action(request):
    user, email, password = authenticate(request)

    instance = "http://cloud-test.hdw.mx/gateway/"
    system_id = request.data['systemId']
    source = request.data['source']
    caption = request.data['caption']
    description = request.data['description'] if 'description' in request.data else ""

    rules_query = GeneratedRules.objects.filter(email=email, source=source, caption=caption,
                                                system_id=system_id, direction="Zapier to Nx")
    if not rules_query.exists():
        make_rule("Generic Event", email, password, caption=caption, source=source, description=description)
        rule = GeneratedRules(email=email, system_id=system_id, caption=caption,
                              source=source, direction="Zapier to Nx")
        rule.save()

    if description:
        description = "&description=" + description

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

    url_link = 'http://cloud-dev.hdw.mx/zapier/fire_hook/?system_id=%s&caption=%s' % (system_id, caption)
    rules_query = GeneratedRules.objects.filter(email=email, caption=caption,
                                                system_id=system_id, direction="Nx to Zapier")
    if not rules_query.exists():
        make_rule("Http Action", email, password, caption=caption, zapier_trigger=url_link)
        rule = GeneratedRules(email=email, system_id=system_id, caption=caption, direction="Nx to Zapier")
        rule.save()

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


@api_view(['GET', 'POST'])
@permission_classes((AllowAny, ))
def test_subscribe(request):
    authenticate(request)
    return Response({'data': [{'caption': 'caption'}]})


def make_rule(rule_type, email, password, caption="", description="", source="", zapier_trigger=""):

    if rule_type == "Generic Event":
        action_params = "{\"additionalResources\":[\"{00000000-0000-0000-0000-100000000000}\",\"{\00000000-0000-0000-"
        action_params += "0000-100000000001}\"],\"allUsers\":false,\"durationMs\":5000,\"forced\":true,\"fps\":10,\""
        action_params += "needConfirmation\":false,\"playToClient\":true,\"recordAfter\":0,\"recordBeforeMs\":1000,\""
        action_params += "streamQuality\":\"highest\",\"useSource\":false}"

        event_condition = "{\"caption\":\"%s\",\"description\":\"%s\",\"eventTimestampUsec\":\"0\",\"event"
        event_condition += "Type\":\"undefinedEvent\",\"metadata\":{\"allUsers\":false},\"reasonCode\":\"none\",\""
        event_condition += "resourceName\":\"%s\"}"

        data = {
            "actionParams": action_params,
            "actionResourceIds": [],
            "actionType": "showPopupAction",
            "aggregationPeriod": 0,
            "comment": "Zapier generated rule for Generic Event made by {}".format(email),
            "disabled": False,
            "eventCondition": event_condition % (caption, description, source),
            "eventResourceIds": [],
            "eventState": "Undefined",
            "eventType": "userDefinedEvent",
            "schedule": "",
            "system": False
        }

    elif rule_type == "Http Action":
        action_params = "{\"allUsers\":false,\"durationMs\":5000,\"forced\":true,\"fps\":10,\"needConfirmation\""
        action_params += ":false,\"playToClient\":true,\"recordAfter\":0,\"recordBeforeMs\":1000,\"streamQuality\":\""
        action_params += "highest\",\"url\":\"%s\",\"useSource\":false}"

        event_condition = "{\"caption\":\"%s\",\"description\":\"_bell_on\",\"eventTimestampUsec\":\"0\",\""
        event_condition += "eventType\":\"undefinedEvent\",\"inputPortId\":\"921d6e67-a9c3-4b2e-b1c8-1162b4a7cd01\",\""
        event_condition += "metadata\":{\"allUsers\":false,\"instigators\":[\"{00000000-0000-0000-0000-100000000000}\""
        event_condition += ",\"{00000000-0000-0000-0000-100000000001}\"]},\"reasonCode\":\"none\"}"

        data = {
            "actionParams": action_params % zapier_trigger,
            "actionResourceIds": [],
            "actionType": "execHttpRequestAction",
            "aggregationPeriod": 0,
            "comment": "Zapier Generated Rule for HTTP action made by {}".format(email),
            "disabled": False,
            "eventCondition": event_condition % caption,
            "eventResourceIds": [],
            "eventState": "Undefined",
            "eventType": "softwareTriggerEvent",
            "id": "{88618778-52af-4553-9bb8-c3831dcaf6bd}",
            "schedule": "",
            "system": False
        }

    else:
        return False

    url = "http://cloud-test.hdw.mx/gateway/9aeb4e34-d495-4e93-8ae5-3af9630ffc0e/ec2/saveEventRule"
    headers = {'Content-Type': 'application/json'}

    requests.post(url, json=data, headers=headers, auth=HTTPDigestAuth(email, password))
    return True