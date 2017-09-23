import django
from rest_framework.authentication import BasicAuthentication
from rest_framework.decorators import api_view, permission_classes, authentication_classes
from rest_framework.permissions import AllowAny
from rest_framework.response import Response
from api.helpers.exceptions import handle_exceptions, APINotAuthorisedException, require_params

import requests, json, base64
from requests.auth import HTTPDigestAuth
from django.http import JsonResponse


def authenticate(request):
    user, email, password = None, None, None
    content = {'user': unicode(request.user), 'auth': unicode(request.auth)}
    return Response(content)

    if "HTTP_AUTHORIZATION" in request.META:
        credentials = request.META['HTTP_AUTHORIZATION'].split()
        if credentials[0].lower() == "basic":
            email, password = base64.b64decode(credentials[1]).split(':', 1)
            user = django.contrib.auth.authenticate(username=email, password=password)

    if user is None:
        raise APINotAuthorisedException('Username or password are invalid')

    return email, password


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
@authentication_classes((BasicAuthentication, ))
@permission_classes((AllowAny, ))
@handle_exceptions
def nx_action(request):
    email, password = authenticate(request)

    instance = "https://cloud-test.hdw.mx/gateway/"
    system_id = request.data['systemId']

    source = 'source=' + request.data['source']
    caption = '&caption=' + request.data['caption']
    state = '&state=' + request.data['state']

    url = "%s%s%s%s%s%s%s" % (instance, system_id, '/api/createEvent?', source, caption, state)

    headers = {'Content-Type': 'application/json'}
    r = requests.get(url, data=None, headers=headers, auth=HTTPDigestAuth(email, password))

    return json.loads(r.text)


@api_view(['GET', 'POST'])
@authentication_classes((BasicAuthentication, ))
@permission_classes((AllowAny, ))
@handle_exceptions
def zap_trigger(request):
    email, password = authenticate(request)

    return Response('ok')


@api_view(['GET', 'POST'])
@authentication_classes((BasicAuthentication, ))
@permission_classes((AllowAny, ))
def ping(request):
    email, password = authenticate(request)

    return JsonResponse({'status': 'ok'})