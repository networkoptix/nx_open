import django, requests, json, base64
from rest_framework.response import Response
from rest_framework.decorators import api_view, permission_classes
from rest_framework.permissions import AllowAny

from api.helpers.exceptions import handle_exceptions, api_success, APINotAuthorisedException

from requests.auth import HTTPDigestAuth
from django.utils.http import urlencode

from rest_hooks.signals import raw_hook_event

from models import *
from cloud import settings

CLOUD_DB_URL = 'http://cloud-test.hdw.mx:80/cdb'
CLOUD_INSTANCE_URL = 'http://cloud-test.hdw.mx'


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


@api_view(['GET'])
@permission_classes((AllowAny, ))
@handle_exceptions
def get_systems(request):
    user, email, password = authenticate(request)
    request = CLOUD_DB_URL + "/system/get?customization=" + settings.CUSTOMIZATION
    data = requests.get(request, auth=HTTPDigestAuth(email, password))
    zap_list = {'systems': []}

    if data:
        data = json.loads(data.text)

    for i in range(len(data['systems'])):
        zap_list['systems'].append({'name': data['systems'][i]['name'], 'system_id': data['systems'][i]['id']})

    return api_success(zap_list)

@api_view(['POST'])
@permission_classes((AllowAny, ))
@handle_exceptions
def zapier_send_generic_event(request):
    user, email, password = authenticate(request)
    system_id = request.data['systemId']
    source = request.data['source']
    caption = request.data['caption']

    query_params = {"source": source, "caption": caption}

    description = request.data['description'] if 'description' in request.data else ""

    if description:
        query_params['description'] = description

    rules_query = GeneratedRule.objects.filter(email=email, source=source, caption=caption,
                                               system_id=system_id, direction="Zapier to Nx")
    if not rules_query.exists():
        make_rule("Generic Event", email, password, system_id, caption=caption, source=source, description=description)
        GeneratedRule(email=email, system_id=system_id, caption=caption, source=source, direction="Zapier to Nx").save()
    else:
        rule = rules_query[0]
        rule.times_used += 1
        rule.save()

    url = "{}/gateway/{}/api/createEvent?{}"\
        .format(CLOUD_INSTANCE_URL, system_id, urlencode(query_params))

    headers = {'Content-Type': 'application/json'}
    r = requests.get(url, data=None, headers=headers, auth=HTTPDigestAuth(email, password))

    return json.loads(r.text)


@api_view(['GET'])
@permission_classes((AllowAny, ))
@handle_exceptions
def nx_http_action(request):
    caption = request.query_params['caption']
    system_id = request.query_params['system_id']
    event = system_id + ' ' + caption
    hooks_event = ZapHook.objects.filter(event=event)

    if hooks_event.exists():
        for hook in hooks_event:
            raw_hook_event.send(sender=None, event_name=event, payload={'data': {'caption': caption}}, user=hook.user)
            query = GeneratedRule.objects.filter(email=hook.user.email, caption=caption,
                                                 system_id= system_id, direction="Nx to Zapier")
            if query.exists():
                rule = query[0]
                rule.times_used += 1
                rule.save()

        return Response({'message': "Webhook fired for " + caption}, status=200)

    else:
        return Response({'message': "Webhook for " + caption + " does not exist"}, status=404)


@api_view(['GET'])
@permission_classes((AllowAny, ))
@handle_exceptions
def ping(request):
    authenticate(request)
    return Response({'status': 'ok'})


@api_view(['POST'])
@permission_classes((AllowAny, ))
@handle_exceptions
def create_zap_webhook(request):
    user, email, password = authenticate(request)

    system_id = request.query_params['system_id']
    caption = request.query_params['caption']
    target = request.data['target_url']

    event = system_id + " " + caption

    query_params = {"system_id": system_id, "caption": caption}

    user_hooks = ZapHook.objects.filter(user=user, target=target)
    if user_hooks.exists():
        return Response({'message': 'There is already a webhook for ' + caption, 'link': None}, status=500)

    url_link = '{}/zapier/trigger_zapier/?{}'.format(CLOUD_INSTANCE_URL, urlencode(query_params))
    rules_query = GeneratedRule.objects.filter(email=email, caption=caption,
                                                system_id=system_id, direction="Nx to Zapier")
    if not rules_query.exists():
        make_rule("Http Action", email, password, system_id, caption=caption, zapier_trigger=url_link)
        GeneratedRule(email=email, system_id=system_id, caption=caption, direction="Nx to Zapier", times_used=0).save()

    zap_hook = ZapHook(user=user, event=event, target=target)
    zap_hook.save()
    return Response({'message': 'Webhook created for ' + caption, 'link': url_link}, status=200)


@api_view(['POST'])
@permission_classes((AllowAny, ))
@handle_exceptions
def remove_zap_webhook(request):
    user, email, password = authenticate(request)
    target = request.data['target_url']

    user_hooks = ZapHook.objects.filter(user=user, target=target)
    if not user_hooks.exists():
        return Response({'message': "Webhook for " + target + " does not exist"}, status=500)

    event = user_hooks[0].event
    user_hooks.delete()
    return Response({'message': 'Webhook deleted for ' + event}, status=200)


@api_view(['GET', 'POST'])
@permission_classes((AllowAny, ))
@handle_exceptions
def test_subscribe(request):
    authenticate(request)
    return Response({'data': [{'caption': 'caption'}]})


def make_rule(rule_type, email, password, system_id, caption="", description="", source="", zapier_trigger=""):

    if rule_type == "Generic Event":
        action_params = "{\"additionalResources\":[\"{00000000-0000-0000-0000-100000000000}\",\"{00000000-0000-0000-" \
                        "0000-100000000001}\"],\"allUsers\":false,\"durationMs\":5000,\"forced\":true,\"fps\":10,\"" \
                        "needConfirmation\":false,\"playToClient\":true,\"recordAfter\":0,\"recordBeforeMs\":1000,\"" \
                        "streamQuality\":\"highest\",\"useSource\":false}"

        event_condition = "{\"caption\":\"%s\",\"description\":\"%s\",\"eventTimestampUsec\":\"0\",\"event" \
                          "Type\":\"undefinedEvent\",\"metadata\":{\"allUsers\":false},\"reasonCode\":\"none\",\"" \
                          "resourceName\":\"%s\"}"

        data = {
            "actionParams": action_params,
            "actionResourceIds": [],
            "actionType": "showPopupAction",
            "aggregationPeriod": 0,
            "comment": "Auto generated rule for Generic Event from Zapier made by {}".format(email),
            "disabled": False,
            "eventCondition": event_condition % (caption, description, source),
            "eventResourceIds": [],
            "eventState": "Undefined",
            "eventType": "userDefinedEvent",
            "schedule": "",
            "system": False
        }

    elif rule_type == "Http Action":
        action_params = "{\"allUsers\":false,\"durationMs\":5000,\"forced\":true,\"fps\":10,\"needConfirmation\"" \
                        ":false,\"playToClient\":true,\"recordAfter\":0,\"recordBeforeMs\":1000,\"streamQuality\":\"" \
                        "highest\",\"url\":\"%s\",\"useSource\":false}"

        event_condition = "{\"caption\":\"Soft Trigger Send %s to Zapier\",\"description\":\"_bell_on\",\"" \
                          "eventTimestampUsec\":\"0\",\"eventType\":\"undefinedEvent\",\"inputPortId\":\"" \
                          "921d6e67-a9c3-4b2e-b1c8-1162b4a7cd01\",\"metadata\":{\"allUsers\":false,\"instigators\"" \
                          ":[\"{00000000-0000-0000-0000-100000000000}\",\"{00000000-0000-0000-0000-100000000001}\"" \
                          "]},\"reasonCode\":\"none\"}"

        data = {
            "actionParams": action_params % zapier_trigger,
            "actionResourceIds": [],
            "actionType": "execHttpRequestAction",
            "aggregationPeriod": 0,
            "comment": "Auto generated rule for HTTP action to Zapier made by {}".format(email),
            "disabled": False,
            "eventCondition": event_condition % caption,
            "eventResourceIds": [],
            "eventState": "Undefined",
            "eventType": "softwareTriggerEvent",
            "schedule": "",
            "system": False
        }

    else:
        return

    url = "{}/gateway/{}/ec2/saveEventRule".format(CLOUD_INSTANCE_URL, system_id)
    headers = {'Content-Type': 'application/json'}

    requests.post(url, json=data, headers=headers, auth=HTTPDigestAuth(email, password))
