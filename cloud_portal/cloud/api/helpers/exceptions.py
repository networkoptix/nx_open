from rest_framework.response import Response
from rest_framework.request import Request
from rest_framework import status
from django.conf import settings
from django.contrib.auth.models import AnonymousUser
from django.http import QueryDict
from enum import Enum
import logging
import json
import traceback

logger = logging.getLogger(__name__)


class ErrorCodes(Enum):
    ok = 'ok'

    # not authorizes

    # Cloud DB errors:
    cloud_invalid_response = 'cloudInvalidResponse'

    # Portal critical errors (unexpected)
    portal_critical_error = 'portalError'

    # Validation errors
    not_authorized = 'notAuthorized'
    wrong_parameters = 'wrongParameters'
    wrong_code = 'wrongCode'
    wrong_old_password = 'wrongOldPassword'

    # CLOUD DB specific errors
    forbidden = 'forbidden'
    account_not_activated = 'accountNotActivated'
    account_blocked = 'accountBlocked'
    not_found = 'notFound'  # Account not found, activation code not found and so on
    account_exists = 'alreadyExists'
    db_error = 'dbError'
    network_error = 'networkError'
    not_implemented = 'notImplemented'
    unknown_realm = 'unknownRealm'
    bad_username = 'badUsername'
    bad_request = 'badRequest'
    invalid_nonce = 'invalidNonce'
    service_unavailable = 'serviceUnavailable'
    unknown_error = 'unknownError'

    response_serialization_error = 'responseSerializationError'
    deserialization_error = 'deserializationError'
    not_acceptable = 'notAcceptable'

    def log_level(self):
        if self in (ErrorCodes.ok,
                    ErrorCodes.not_authorized,
                    ErrorCodes.not_found,
                    ErrorCodes.account_exists,
                    ErrorCodes.wrong_old_password,
                    ErrorCodes.account_not_activated):
            return logging.INFO
        if self in (ErrorCodes.forbidden,
                    ErrorCodes.wrong_code):
            return logging.WARNING
        return logging.ERROR


def api_success(data=None, status_code=status.HTTP_200_OK):
    if data is not None:
        return Response(data, status=status_code)
    return Response({
                'resultCode': ErrorCodes.ok.value
            }, status=status_code)


def require_params(request, params_list):
    error_data = {}
    for param in params_list:
        if param not in request.data or request.data[param] == '':
            error_data[param] = ['This field is required.']

    if error_data:
        raise APIRequestException('Parameters are missing', ErrorCodes.wrong_parameters,
                                  error_data=error_data)


class APIException(Exception):
    error_text = 'Unexpected error'
    error_code = ErrorCodes.unknown_error
    error_data = None
    status_code = status.HTTP_500_INTERNAL_SERVER_ERROR

    def __init__(self, error_text, error_code, error_data=None,
                 status_code=status.HTTP_500_INTERNAL_SERVER_ERROR):
        if error_code is None:
            error_code = status_code

        if isinstance(error_code, basestring):
            try:
                error_code = ErrorCodes(error_code)
            except ValueError:
                logger.error('Unexpected error code {0}'.format(error_code))

        self.error_data = error_data
        self.error_code = error_code
        self.error_text = error_text
        self.status_code = status_code
        super(Exception, self).__init__(error_text)

    def response(self):
        result_code = self.error_code
        if isinstance(self.error_code, ErrorCodes):
            result_code = self.error_code.value

        return Response({
                'resultCode': result_code,
                'errorText': self.error_text,
                'errorData': self.error_data
            }, status=self.status_code)

    def log_level(self):
        if isinstance(self.error_code, basestring):
            return logging.ERROR
        return self.error_code.log_level()


class APIInternalException(APIException):
    # 500 error - internal server error
    def __init__(self, error_text, error_code, error_data=None):
        super(APIInternalException, self).__init__(error_text, error_code, error_data=error_data,
                                                   status_code=status.HTTP_500_INTERNAL_SERVER_ERROR)


class APIServiceException(APIException):
    # 503 error - service unavailable
    def __init__(self, error_text, error_code, error_data=None):
        super(APIServiceException, self).__init__(error_text, error_code, error_data=error_data,
                                                  status_code=status.HTTP_503_SERVICE_UNAVAILABLE)


class APINotFoundException(APIException):
    # 404 error - service unavailable
    def __init__(self, error_text, error_code, error_data=None):
        super(APINotFoundException, self).__init__(error_text, error_code, error_data=error_data,
                                                   status_code=status.HTTP_404_NOT_FOUND)


class APIForbiddenException(APIException):
    # 403 error - action is forbidden
    def __init__(self, error_text, error_code=ErrorCodes.not_authorized, error_data=None):
        super(APIForbiddenException, self).__init__(error_text, error_code,
                                                    error_data=error_data,
                                                    status_code=status.HTTP_403_FORBIDDEN)


class APINotAuthorisedException(APIException):
    # 401 error - user is not authorized
    def __init__(self, error_text, error_code=ErrorCodes.not_authorized, error_data=None):
        super(APINotAuthorisedException, self).__init__(error_text,
                                                        error_code,
                                                        error_data=error_data,
                                                        status_code=status.HTTP_401_UNAUTHORIZED)


class APIRequestException(APIException):
    # 400 error - bad request
    def __init__(self, error_text, error_code, error_data=None):
        super(APIRequestException, self).__init__(error_text, error_code, error_data=error_data,
                                                  status_code=status.HTTP_400_BAD_REQUEST)


class APILogicException(APIException):
    # 200. but some error occurred
    def __init__(self, error_text, error_code, error_data=None):
        super(APILogicException, self).__init__(error_text, error_code, error_data=error_data,
                                                status_code=status.HTTP_200_OK)


def validate_mediaserver_response(func):
    def validate_error(response_data):
        if 'resultCode' not in response_data or 'errorText' not in response_data:
            raise APIInternalException('No valid error message from gateway', ErrorCodes.cloud_invalid_response)

    def validator(*args, **kwargs):
        response = func(*args, **kwargs)

        if response.status_code == status.HTTP_204_NO_CONTENT:  # No content
            return None

        # 1. Validate JSON response. If not a json - raise exception
        # Treat JSON response problems as Internal Server Errors
        try:
            response_data = response.json()
        except ValueError:
            response_data = None

        errors = {
            status.HTTP_500_INTERNAL_SERVER_ERROR: APIInternalException,
            status.HTTP_503_SERVICE_UNAVAILABLE: APIServiceException,
            status.HTTP_404_NOT_FOUND: APINotFoundException,
            status.HTTP_403_FORBIDDEN: APIForbiddenException,
            status.HTTP_401_UNAUTHORIZED: APINotAuthorisedException,
            status.HTTP_400_BAD_REQUEST: APIRequestException,
        }

        if response.status_code in errors:
            if response_data:
                validate_error(response_data)
                raise errors[response.status_code](response_data['errorText'], error_code=response_data['resultCode'], error_data=response_data)
            else:
                raise errors[response.status_code](response.text, error_code=ErrorCodes.unknown_error)

        # everything is OK - return server's response
        return response_data

    return validator


def validate_response(func):
    def validate_error(response_data):
        if 'resultCode' not in response_data or 'errorText' not in response_data:
            raise APIInternalException('No valid error message from cloud_db', ErrorCodes.cloud_invalid_response)

    def validator(*args, **kwargs):
        response = func(*args, **kwargs)

        if response.status_code == status.HTTP_204_NO_CONTENT:  # No content
            return None

        # 1. Validate JSON response. If not a json - raise exception
        # Treat JSON response problems as Internal Server Errors
        try:
            response_data = response.json()
        except ValueError:
            raise APIInternalException('No JSON data from cloud_db (code:' + str(response.status_code) + ") " +
                                       response.text, ErrorCodes.cloud_invalid_response)

        errors = {
            status.HTTP_500_INTERNAL_SERVER_ERROR: APIInternalException,
            status.HTTP_503_SERVICE_UNAVAILABLE: APIServiceException,
            status.HTTP_404_NOT_FOUND: APINotFoundException,
            # HTTP_403_FORBIDDEN
            status.HTTP_401_UNAUTHORIZED: APINotAuthorisedException,
            status.HTTP_400_BAD_REQUEST: APIRequestException,
        }

        if response.status_code in errors:
            validate_error(response_data)
            raise errors[response.status_code](response_data['errorText'], error_code=response_data['resultCode'])

        logic_errors = {
            ErrorCodes.forbidden: APINotAuthorisedException,
            ErrorCodes.not_authorized: APINotAuthorisedException
        }

        # Check error_code status - raise APILogicException
        if 'resultCode' in response_data and response_data['resultCode'] != ErrorCodes.ok.value:
            validate_error(response_data)
            if response_data['resultCode'] in logic_errors:
                raise logic_errors[response_data['resultCode']](response_data['errorText'], error_code=response_data['resultCode'])
            raise APILogicException(response_data['errorText'], response_data['resultCode'])

        # everything is OK - return server's response
        return response_data

    return validator


def get_client_ip(request):
    x_forwarded_for = request.META.get('HTTP_X_FORWARDED_FOR')
    if x_forwarded_for:
        ip = x_forwarded_for.split(',')[0]
    else:
        ip = request.META.get('REMOTE_ADDR')
    return ip


def handle_exceptions(func):
    """
    Decorator for api_methods to handle all unhandled exception and return some reasonable response for a client
    :param func:
    :return:
    """
    def clean_passwords(dictionary):
        if isinstance(dictionary, dict):
            if 'password' in dictionary:
                dictionary['password'] = '*****'
            if 'new_password' in dictionary:
                dictionary['new_password'] = '****'
            if 'old_password' in dictionary:
                dictionary['old_password'] = '***'

    def log_error(request, error, log_level):
        page_url = 'unknown'
        user_name = 'not authorized'
        request_data = ''
        ip = 'unknown'

        if isinstance(request, Request):
            page_url = request.build_absolute_uri()
            request_data = request.data
            ip = get_client_ip(request)
            if not isinstance(request.user, AnonymousUser):
                user_name = request.user.email

        if isinstance(request_data, QueryDict):
            request_data = request_data.dict()

        if isinstance(error, APIException):
            error_text = "{}({})".format(error.error_text, error.error_code)
            if error.error_data:
                clean_passwords(error.error_data)
            error_formatted = 'Status: {}, Message: {}, Result code: {}, Data: {}'.\
                              format(error.status_code,
                                     error.error_text,
                                     error.error_code,
                                     json.dumps(error.error_data, indent=4, separators=(',', ': '))
                                     )
        else:
            error_text = 'unknown'
            error_formatted = 'Unexpected error'

        clean_passwords(request_data)

        if log_level == logging.INFO:
            error_formatted = ' {}:{} ({} at {}) Request: {}'. \
                format(error.__class__.__name__,
                       error_text,
                       user_name,
                       page_url,
                       request_data
                       )
        else:
            error_formatted = ' {}:{}\n{}({}) at {} Request: {}\n{}\nCall Stack: \n{}'. \
                format(error.__class__.__name__,
                       error_text,
                       user_name,
                       ip,
                       page_url,
                       request_data,
                       error_formatted,
                       traceback.format_exc()
                       ).replace("Traceback", "")  # remove Traceback word from handled exceptions

        logger.log(log_level, error_formatted)
        return error_formatted

    def handler(*args, **kwargs):
        # noinspection PyBroadException
        try:
            data = func(*args, **kwargs)
            if not isinstance(data, Response):
                return Response(data, status=status.HTTP_200_OK)
            return data
        except APIException as error:
            # Do not log not_authorized errors
            log_error(args[0], error, error.log_level())

            return error.response()
        except Exception as error:
            detailed_error = log_error(args[0], error, logging.ERROR)

            if not settings.DEBUG:
                detailed_error = 'Unexpected error somewhere inside'

            return Response({
                'resultCode': status.HTTP_500_INTERNAL_SERVER_ERROR,
                'errorText': detailed_error
            }, status=status.HTTP_500_INTERNAL_SERVER_ERROR)
    return handler
