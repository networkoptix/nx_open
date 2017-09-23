import django
from rest_framework.decorators import api_view, permission_classes
from rest_framework.permissions import AllowAny
from rest_framework.response import Response
from api.helpers.exceptions import handle_exceptions, APINotAuthorisedException, require_params

import requests, json
from requests.auth import HTTPDigestAuth
from django.http import JsonResponse


def authenticate(request):
    require_params(request, ('email', 'password'))
    email = request.data['email'].lower()
    password = request.data['password']
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
@permission_classes((AllowAny, ))
@handle_exceptions
def zap_trigger(request):
    email, password = authenticate(request)

    return Response('ok')


@api_view(['GET', 'POST'])
@permission_classes((AllowAny, ))
def ping(request):
    email, password = authenticate(request)

    return JsonResponse({'status': 'ok'})