from rest_framework.decorators import api_view, permission_classes
from rest_framework.response import Response
from account_serializers import AccountSerializer, CreateAccountSerializer, AccountUpdateSerializer
from rest_framework.permissions import AllowAny, IsAuthenticated
from rest_framework.serializers import ValidationError
import django
import base64
from django.utils import timezone
from django.core.exceptions import ObjectDoesNotExist
import api
from api.controllers.cloud_api import Account
from api.account_backend import AccountBackend
from api.helpers.exceptions import handle_exceptions, APIRequestException, APINotAuthorisedException, \
    APIInternalException, api_success, ErrorCodes, require_params

import logging
logger = logging.getLogger(__name__)


@api_view(['POST'])
@permission_classes((AllowAny, ))
@handle_exceptions
def register(request):
    from .utils import detect_language_by_request
    logger.debug('/api/account/register called')
    lang = detect_language_by_request(request)
    data = request.data
    data['language'] = lang
    AccountBackend.check_email_in_portal(data['email'], False)  # Check if account is in Cloud_db
    serializer = CreateAccountSerializer(data=data)
    if not serializer.is_valid():
        raise APIRequestException('Wrong form parameters', ErrorCodes.wrong_parameters, error_data=serializer.errors)
    logger.debug('/api/account/register calling serializer.save')
    serializer.save()
    logger.debug('/api/account/register completed')
    return api_success()


@api_view(['POST'])
@permission_classes((AllowAny, ))
@handle_exceptions
def login(request):
    user = None
    if 'login' in request.session and 'password' in request.session:
        email = request.session['login']
        password = request.session['password']
        try:
            user = django.contrib.auth.authenticate(username=email, password=password)
        except APINotAuthorisedException:
            pass

    if user is None:
        # authorize user here
        # return user
        require_params(request, ('email', 'password'))
        email = request.data['email'].lower()
        password = request.data['password']
        user = django.contrib.auth.authenticate(username=email, password=password)

    if user is None:
        raise APINotAuthorisedException('Username or password are invalid')

    if 'remember' not in request.data or not request.data['remember']:
        request.session.set_expiry(0)

    django.contrib.auth.login(request, user)
    request.session['login'] = email
    request.session['password'] = password
    # TODO: This is awful security hole! But I can't remove it now, because I need password for future requests

    serializer = AccountSerializer(user, many=False)
    return api_success(serializer.data)


@api_view(['POST'])
@permission_classes((IsAuthenticated, ))
@handle_exceptions
def logout(request):
    request.session.pop('login', None)
    request.session.pop('password', None)
    django.contrib.auth.logout(request)
    return api_success()


@api_view(['GET', 'POST'])
@permission_classes((IsAuthenticated, ))
@handle_exceptions
def index(request):
    if request.method == 'GET':
        # validate credentials in cloud_db
        # password could be changed, ot temporary link expired
        Account.get(request.session['login'], request.session['password'])
        # get authorized user here
        serializer = AccountSerializer(request.user, many=False)
        return Response(serializer.data)

    elif request.method == 'POST':
        serializer = AccountUpdateSerializer(request.user, data=request.data)
            
        if not serializer.is_valid():
            raise APIRequestException('Wrong form parameters',
                                      ErrorCodes.wrong_parameters,
                                      error_data=serializer.errors)

        Account.update(request.session['login'], request.session['password'], request.data['first_name'],
                       request.data['last_name'])
        # if not success:
        #    return Response(serializer.errors, status=status.HTTP_400_BAD_REQUEST)
        serializer.save()
        return api_success(serializer.data)


@api_view(['POST'])
@permission_classes((IsAuthenticated,))
@handle_exceptions
def auth_key(request):
    data = Account.create_temporary_credentials(request.session['login'], request.session['password'], 'short')

    key = base64.b64encode(data['login'] + ':' + data['password'])
    return api_success({'auth_key': key})


@api_view(['POST'])
@permission_classes((IsAuthenticated, ))
@handle_exceptions
def change_password(request):
    require_params(request, ('old_password', 'new_password'))
    old_password = request.data['old_password']
    new_password = request.data['new_password']

    try:
        CreateAccountSerializer.validate_password(new_password)
    except ValidationError as error:
        raise APIRequestException('Incorrect new password', ErrorCodes.wrong_parameters,
                                  error_data={'new_password': error.detail})

    try:
        Account.change_password(request.user.email, old_password, new_password)
    except APINotAuthorisedException as error:
        raise APIRequestException('Wrong old password', ErrorCodes.wrong_old_password,
                                  error_data={'old_password': error.error_data})

    request.session['password'] = new_password
    return api_success()


@api_view(['POST'])
@permission_classes((AllowAny, ))
@handle_exceptions
def activate(request):
    if 'code' in request.data:
        code = request.data['code']
        user_data = Account.activate(code)

        if 'email' not in user_data:
            raise APIInternalException('No email from cloud_db', ErrorCodes.cloud_invalid_response)

        email = user_data['email'].lower()
        try:
            user = api.models.Account.objects.get(email=email)
            user.activated_date = timezone.now()
            user.save(update_fields=['activated_date'])
        except ObjectDoesNotExist:
            raise APIInternalException('No email in portal_db', ErrorCodes.portal_critical_error)
        return api_success()
    elif 'user_email' in request.data:
        user_email = request.data['user_email'].lower()
        AccountBackend.check_email_in_portal(user_email, True)  # Check if account is in Cloud_db
        Account.reactivate(user_email)
    else:
        raise APIRequestException('Parameters are missing', ErrorCodes.wrong_parameters,
                                  error_data={'code': ['This field is required.'],
                                              'user_email': ['This field is required.']})

    return api_success()


@api_view(['POST'])
@permission_classes((AllowAny, ))
@handle_exceptions
def restore_password(request):
    if 'code' in request.data:
        code = request.data['code']
        new_password = request.data['new_password']
        try:
            CreateAccountSerializer.validate_password(new_password)
        except ValidationError as error:
            raise APIRequestException('Wrong new password', ErrorCodes.wrong_parameters,
                                      error_data={'new_password': error.detail})

        Account.restore_password(code, new_password)
    elif 'user_email' in request.data:
        user_email = request.data['user_email'].lower()
        AccountBackend.check_email_in_portal(user_email, True)  # Check if account is in Cloud_db

        Account.reset_password(user_email)
    else:
        raise APIRequestException('Parameters are missing', ErrorCodes.wrong_parameters,
                                  error_data={'code': ['This field is required.'],
                                              'user_email': ['This field is required.']})
    return api_success()
