import django, json, base64, urllib

from rest_framework.response import Response
from rest_framework.decorators import api_view, permission_classes
from rest_framework.permissions import AllowAny

from django.utils.http import urlencode

from api.helpers.exceptions import handle_exceptions, api_success, APINotAuthorisedException
from api.controllers import cloud_api, cloud_gateway

from models import *
from cloud import settings
from html_sanitizer import Sanitizer
sanitizer = Sanitizer()

CLOUD_DB_URL = settings.CLOUD_CONNECT['url']
CLOUD_INSTANCE_URL = settings.conf['cloud_portal']['url']


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


def increment_rule(rule):
    rule.times_used += 1
    rule.save()


def make_rule(rule_type, email, password, system_id, caption="", description="", source="", zapier_trigger=""):
    if rule_type == "Generic Event":
        action_params = json.dumps({"additionalResources": ["{00000000-0000-0000-0000-100000000000}",
                                                "{00000000-0000-0000-0000-100000000001}"],
                                    "allUsers": False,
                                    "durationMs": 5000,
                                    "forced": True,
                                    "fps": 10,
                                    "needConfirmation": False,
                                    "playToClient": True,
                                    "recordAfter": 0,
                                    "recordBeforeMs": 1000,
                                    "streamQuality": "highest",
                                    "useSource": False
                                    })

        event_condition = json.dumps({"caption": caption,
                                      "description": description,
                                      "eventTimestampUsec": "0",
                                      "eventType": "undefinedEvent",
                                      "metadata": {
                                          "allUsers": False
                                      },
                                      "reasonCode": "none",
                                      "resourceName": source
                                      })

        data = {
            "actionParams": action_params,
            "actionResourceIds": [],
            "actionType": "showPopupAction",
            "aggregationPeriod": 0,
            "comment": "Auto generated rule for Generic Event from Zapier made by {}".format(email),
            "disabled": False,
            "eventCondition": event_condition,
            "eventResourceIds": [],
            "eventState": "Undefined",
            "eventType": "userDefinedEvent",
            "schedule": "",
            "system": False
        }

    elif rule_type == "Http Action":
        action_params = json.dumps({"allUsers": False,
                                    "durationMs": 5000,
                                    "forced": True,
                                    "fps": 10,
                                    "needConfirmation": False,
                                    "playToClient": True,
                                    "recordAfter": 0,
                                    "recordBeforeMs": 1000,
                                    "streamQuality": "highest",
                                    "url": zapier_trigger,
                                    "useSource": False
                                    })

        event_condition = json.dumps({"caption": "Soft Trigger Send " + caption + " to Zapier",
                                      "description": "_bell_on",
                                      "eventTimestampUsec": "0",
                                      "eventType": "undefinedEvent",
                                      "inputPortId": "921d6e67-a9c3-4b2e-b1c8-1162b4a7cd01",
                                      "metadata": {
                                          "allUsers": False,
                                          "instigators": ["{00000000-0000-0000-0000-100000000000}",
                                                          "{00000000-0000-0000-0000-100000000001}"]
                                      },
                                      "reasonCode": "none"
                                      })

        data = {
            "actionParams": action_params,
            "actionResourceIds": [],
            "actionType": "execHttpRequestAction",
            "aggregationPeriod": 0,
            "comment": "Auto generated rule for HTTP action to Zapier made by {}".format(email),
            "disabled": False,
            "eventCondition": event_condition,
            "eventResourceIds": [],
            "eventState": "Undefined",
            "eventType": "softwareTriggerEvent",
            "schedule": "",
            "system": False
        }

    else:
        return

    cloud_gateway.post('gateway/' + system_id, "ec2/saveEventRule", data, email, password)


def make_or_increment_rule(action, email, system_id, caption, password=None,
                           description=None, source=None, target_url=None):
    rules_query = GeneratedRule.objects.filter(email=email, system_id=system_id, caption=caption)

    if action == 'Generic Event':
        rules_query = rules_query.filter(source=source, direction="Zapier to Nx")

        if not rules_query.exists():
            make_rule(action, email, password, system_id,
                      caption=caption, source=source, description=description)
            GeneratedRule(email=email, system_id=system_id, caption=caption,
                          source=source, direction="Zapier to Nx").save()

        else:
            increment_rule(rules_query[0])

    elif action == 'Http Action':
        rules_query = rules_query.filter(direction="Nx to Zapier")
        if not rules_query.exists():
            make_rule(action, email, password, system_id, caption=caption, zapier_trigger=target_url)
            GeneratedRule(email=email, system_id=system_id, caption=caption, direction="Nx to Zapier",
                          times_used=0).save()

    elif action == 'Hook Fired':
        rules_query = rules_query.filter(direction="Nx to Zapier")

        if rules_query.exists():
            increment_rule(rules_query[0])


@api_view(['GET'])
@permission_classes((AllowAny, ))
@handle_exceptions
def get_systems(request):
    user, email, password = authenticate(request)
    data = cloud_api.System.list(email, password)
    zap_list = {'systems': []}

    for system in data['systems']:
        zap_list['systems'].append({'name': system['name'], 'system_id': system['id']})

    return api_success(zap_list)

@api_view(['POST'])
@permission_classes((AllowAny, ))
@handle_exceptions
def zapier_send_generic_event(request):
    user, email, password = authenticate(request)
    system_id = request.data['systemId']
    source = sanitizer.sanitize(request.data['source'])
    caption = sanitizer.sanitize(request.data['caption'])

    query_params = {"source": source, "caption": caption}

    description = sanitizer.sanitize(request.data['description']) if 'description' in request.data else ""

    if description:
        query_params['description'] = description

    make_or_increment_rule('Generic Event', email, system_id, caption,
                           password=password, description=description, source=source)

    url = "api/createEvent?{}".format(urllib.urlencode(query_params).replace('+', "%20"))
    return cloud_gateway.get('gateway/' + system_id, url, email, password)


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
            hook.deliver_hook(None, {'caption': caption})
            make_or_increment_rule('Hook Fired', hook.user.email, system_id, caption)

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
def subscribe_webhook(request):
    user, email, password = authenticate(request)

    system_id = request.query_params['system_id']
    caption = sanitizer.sanitize(request.query_params['caption'])
    target = request.data['target_url']

    event = system_id + " " + caption

    query_params = {"system_id": system_id, "caption": caption}

    user_hooks = ZapHook.objects.filter(user=user, target=target)
    if user_hooks.exists():
        return Response({'message': 'There is already a webhook for ' + caption, 'link': None}, status=500)

    url_link = '{}/zapier/?{}'.format(CLOUD_INSTANCE_URL, urlencode(query_params))

    make_or_increment_rule('Http Action', email, system_id, caption, password=password, target_url=url_link)

    zap_hook = ZapHook(user=user, event=event, target=target)
    zap_hook.save()
    return Response({'message': 'Webhook created for ' + caption, 'link': url_link}, status=200)


@api_view(['POST'])
@permission_classes((AllowAny, ))
@handle_exceptions
def unsubscribe_webhook(request):
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
