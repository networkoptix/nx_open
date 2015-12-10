from rest_framework import status
from rest_framework.decorators import api_view, permission_classes
from rest_framework.response import Response
from account_serializers import AccountSerializer, CreateAccountSerializer, AccountUpdaterSerializer
from rest_framework.permissions import AllowAny, IsAuthenticated
import django
import base64
import logging
from api.controllers.cloud_api import Account
from api.helpers.exceptions import handle_exceptions, APIRequestException, APINotAuthorisedException, api_success

__django__ = logging.getLogger('django')


@api_view(['POST'])
@permission_classes((AllowAny, ))
@handle_exceptions
def register(request):
    serializer = CreateAccountSerializer(data=request.data)
    if not serializer.is_valid():
        raise APIRequestException('Wrong form parameters', error_data=serializer.errors)
    serializer.save()
    return api_success()


@api_view(['POST'])
@permission_classes((AllowAny, ))
@handle_exceptions
def login(request):
    # authorize user here
    # return user

    user = django.contrib.auth.authenticate(username=request.data['email'], password=request.data['password'])
    if user is None:
        raise APINotAuthorisedException('Username or password are invalid')

    django.contrib.auth.login(request, user)
    request.session['password'] = request.data['password']
    # TODO: This is awful security hole! But I can't remove it now, because I need password for future requests

    serializer = AccountSerializer(user, many=False)
    return api_success(serializer.data)


@api_view(['POST'])
@permission_classes((IsAuthenticated, ))
@handle_exceptions
def logout(request):
    django.contrib.auth.logout(request)
    return api_success()


@api_view(['GET', 'POST'])
@permission_classes((IsAuthenticated, ))
@handle_exceptions
def index(request):

    __django__.debug("index")

    """
    List all snippets, or create a new snippet.
    """
    if request.method == 'GET':
        # get authorized user here
        serializer = AccountSerializer(request.user, many=False)
        return Response(serializer.data)

    elif request.method == 'POST':
        serializer = AccountUpdaterSerializer(request.user, data=request.data)
            
        if not serializer.is_valid():
            raise APIRequestException(serializer.errors)
        
        Account.update(request.user.email, request.session['password'], request.user.first_name, request.user.last_name)
        # if not success:
        #    return Response(serializer.errors, status=status.HTTP_400_BAD_REQUEST)
        serializer.save()
        return api_success(serializer.data)


@api_view(['POST'])
@permission_classes((IsAuthenticated, ))
@handle_exceptions
def change_password(request):
    old_password = request.data['old_password']
    new_password = request.data['new_password']

    Account.change_password(request.user.email, old_password, new_password)
    request.session['password'] = new_password
    return api_success()


@api_view(['POST'])
@permission_classes((AllowAny, ))
@handle_exceptions
def activate(request):
    if 'code' not in request.data:
        raise APIRequestException('Activation code is absent')
    code = request.data['code']
    Account.activate(code)
    return api_success()


@api_view(['POST'])
@permission_classes((AllowAny, ))
@handle_exceptions
def restore_password(request):

    if 'code' not in request.data and 'user_email' not in request.data :
        raise APIRequestException('Required parameters are absent')

    if 'code' in request.data:
        code = request.data['code']

        try:
            (temp_password, user_email) = base64.b64decode(code).split(":")
        except TypeError:
            raise APIRequestException('Activation code has wrong structure')

        if not user_email or user_email is None or user_email == '':
            raise APIRequestException('Activation code has wrong structure',
                                      {'code': code})

        if 'new_password' not in request.data:
            raise APIRequestException('New password is absent')

        new_password = request.data['new_password']

        if new_password == '':
            raise APIRequestException('New password is empty')

        Account.change_password(user_email, temp_password, new_password)
    else:
        user_email = request.data['user_email']
        Account.reset_password(user_email)

    return api_success()
