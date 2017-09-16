import django
from rest_framework.decorators import api_view, permission_classes
from rest_framework.permissions import AllowAny
from rest_framework.response import Response
from api.helpers.exceptions import handle_exceptions, APINotAuthorisedException, require_params


def authenticate(request):
    require_params(request, ('email', 'password'))
    email = request.data['email'].lower()
    password = request.data['password']
    user = django.contrib.auth.authenticate(username=email, password=password)

    if user is None:
        raise APINotAuthorisedException('Username or password are invalid')

    return user


@api_view(['GET', 'POST'])
@permission_classes((AllowAny, ))
@handle_exceptions
def nx_action(request):
    # authorize user here
    # return user
    user = authenticate(request)

    return Response('ok')


@api_view(['GET', 'POST'])
@permission_classes((AllowAny, ))
@handle_exceptions
def zap_trigger(request):
    user = authenticate(request)

    return Response('ok')


@api_view(['GET', 'POST'])
@permission_classes((AllowAny, ))
def ping(request):
    return Response('ok')

@api_view(['GET', 'POST'])
@permission_classes((AllowAny, ))
@handle_exceptions
def list_systems(request):
    user = authenticate(request)

    return Response('ok')