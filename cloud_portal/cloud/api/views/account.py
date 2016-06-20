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
from api.helpers.exceptions import handle_exceptions, APIRequestException, APINotAuthorisedException, \
    APIInternalException, api_success, ErrorCodes, require_params


@api_view(['POST'])
@permission_classes((AllowAny, ))
@handle_exceptions
def register(request):
    serializer = CreateAccountSerializer(data=request.data)
    if not serializer.is_valid():
        raise APIRequestException('Wrong form parameters', ErrorCodes.wrong_parameters, error_data=serializer.errors)
    serializer.save()
    return api_success()


@api_view(['POST'])
@permission_classes((AllowAny, ))
@handle_exceptions
def login(request):
    # authorize user here
    # return user
    require_params(request, ('email', 'password'))
    user = django.contrib.auth.authenticate(username=request.data['email'], password=request.data['password'])
    if user is None:
        raise APINotAuthorisedException('Username or password are invalid')

    if 'remember' not in request.data or not request.data['remember']:
        request.session.set_expiry(0)

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
    if request.method == 'GET':
        # get authorized user here
        serializer = AccountSerializer(request.user, many=False)
        return Response(serializer.data)

    elif request.method == 'POST':
        serializer = AccountUpdateSerializer(request.user, data=request.data)
            
        if not serializer.is_valid():
            raise APIRequestException('Wrong form parameters',
                                      ErrorCodes.wrong_parameters,
                                      error_data=serializer.errors)

        Account.update(request.user.email, request.session['password'], request.data['first_name'],
                       request.data['last_name'])
        # if not success:
        #    return Response(serializer.errors, status=status.HTTP_400_BAD_REQUEST)
        serializer.save()
        return api_success(serializer.data)


@api_view(['POST'])
@permission_classes((IsAuthenticated,))
@handle_exceptions
def auth_key(request):
    data = Account.create_temporary_credentials('short')

    key = base64.b64encode(data['login'] + ':' + data['password'])
    return api_success(request.user.email, request.session['password'], {'auth_key': key})


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
        raise APIRequestException('Wrong new password', ErrorCodes.wrong_parameters,
                                  error_data={'new_password': error.detail})

    Account.change_password(request.user.email, old_password, new_password)
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

        email = user_data['email']
        try:
            user = api.models.Account.objects.get(email=email)
            user.activated_date = timezone.now()
            user.save(update_fields=['activated_date'])
        except ObjectDoesNotExist:
            raise APIInternalException('No email in portal_db', ErrorCodes.portal_critical_error)
        return api_success()
    elif 'user_email' in request.data:
        Account.reactivate(request.data['user_email'])
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

        try:
            (temp_password, user_email) = base64.b64decode(code).split(":")
        except TypeError:
            raise APIRequestException('Activation code has wrong structure:' + code, ErrorCodes.wrong_code)
        except ValueError:
            raise APIRequestException('Activation code has wrong structure:' + code, ErrorCodes.wrong_code)
        if not user_email or not temp_password:
            raise APIRequestException('Activation code has wrong structure:' + code, ErrorCodes.wrong_code)

        require_params(request, ('new_password',))

        new_password = request.data['new_password']
        try:
            CreateAccountSerializer.validate_password(new_password)
        except ValidationError as error:
            raise APIRequestException('Wrong new password', ErrorCodes.wrong_parameters,
                                      error_data={'new_password': error.detail})

        Account.change_password(user_email, temp_password, new_password)
    elif 'user_email' in request.data:
        user_email = request.data['user_email']
        Account.reset_password(user_email)
    else:
        raise APIRequestException('Parameters are missing', ErrorCodes.wrong_parameters,
                                  error_data={'code': ['This field is required.'],
                                              'user_email': ['This field is required.']})
    return api_success()
