from rest_framework.response import Response
from rest_framework import status
from django.conf import settings
import logging
import json
import traceback

__django__ = logging.getLogger('django')


def api_success(data=None, status_code=status.HTTP_200_OK):
    if data is not None:
        return Response(data, status=status_code)
    return Response({
                'resultCode': 0
            }, status=status_code)


class APIException(Exception):
    error_text = 'Unexpected error'
    error_code = status.HTTP_500_INTERNAL_SERVER_ERROR
    error_data = None
    status_code = status.HTTP_500_INTERNAL_SERVER_ERROR

    def __init__(self, error_text, error_code=None, error_data=None,
                 status_code=status.HTTP_500_INTERNAL_SERVER_ERROR):
        if error_code is None:
            error_code = status_code
        self.error_data = error_data
        self.error_code = error_code
        self.error_text = error_text
        self.status_code = status_code
        super(Exception, self).__init__(error_text)

    def response(self):
        return Response({
                'resultCode': self.error_code,
                'errorText': self.error_text,
                'errorData': self.error_data
            }, status=self.status_code)


class APIInternalException(APIException):
    # 500 error - internal server error
    def __init__(self, error_text, error_data=None, error_code=None):
        super(APIInternalException, self).__init__(error_text, error_data=error_data, error_code=error_code,
                                                   status_code=status.HTTP_500_INTERNAL_SERVER_ERROR)


class APIServiceException(APIException):
    # 503 error - service unavailable
    def __init__(self, error_text, error_data=None, error_code=None):
        super(APIServiceException, self).__init__(error_text, error_data=error_data, error_code=error_code,
                                                  status_code=status.HTTP_503_SERVICE_UNAVAILABLE)


class APINotFoundException(APIException):
    # 404 error - service unavailable
    def __init__(self, error_text, error_data=None, error_code=None):
        super(APINotFoundException, self).__init__(error_text, error_data=error_data, error_code=error_code,
                                                   status_code=status.HTTP_404_NOT_FOUND)


class APINotAuthorisedException(APIException):
    # 401 error - service unavailable
    def __init__(self, error_text, error_data=None, error_code=None):
        super(APINotAuthorisedException, self).__init__(error_text, error_data=error_data, error_code=error_code,
                                                        status_code=status.HTTP_401_UNAUTHORIZED)


class APIRequestException(APIException):
    # 400 error - bad request
    def __init__(self, error_text, error_data=None, error_code=None):
        super(APIRequestException, self).__init__(error_text, error_data=error_data, error_code=error_code,
                                                  status_code=status.HTTP_400_BAD_REQUEST)


class APILogicException(APIException):
    # 200. but some error occurred
    def __init__(self, error_text, error_data=None, error_code=None):
        super(APILogicException, self).__init__(error_text, error_data=error_data, error_code=error_code,
                                                status_code=status.HTTP_200_OK)


def validate_response(func):
    def validate_error(response_data):
        if 'resultCode' not in response_data or 'errorText' not in response_data:
            raise APIInternalException('No valid error message from cloud_db')

    def validator(*args, **kwargs):
        response = func(*args, **kwargs)

        __django__.debug(response.status_code)
        __django__.debug(response.text)
        __django__.debug('------------------------------------------------------------')

        if response.status_code == status.HTTP_204_NO_CONTENT:  # No content
            return None

        # 1. Validate JSON response. If not a json - raise exception
        # Treat JSON response problems as Internal Server Errors
        try:
            response_data = response.json()
        except ValueError:
            raise APIInternalException('No JSON data from cloud_db (code:' + str(response.status_code) + ") " + response.text)

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
            raise errors[response.status_code](response_data['resultCode'], response_data['errorText'])

        # Check error_code status - raise APILogicException
        if 'resultCode' in response_data and response_data['resultCode'] != 0:
            validate_error(response_data)
            raise APILogicException(response_data['errorText'], response_data['resultCode'])

        # everything is OK - return server's response
        return response_data

    return validator


def handle_exceptions(func):
    """
    Decorator for api_methods to handle all unhandled exception and return some reasonable response for a client
    :param func:
    :return:
    """

    def handler(*args, **kwargs):
        # noinspection PyBroadException
        try:
            data = func(*args, **kwargs)
            if not isinstance(data, Response):
                return Response(data, status=status.HTTP_200_OK)
            return data
        except APIException as error:
            __django__.error('APIException. \nStatus: {0}\nMessage: {1}\nError code: {2}\nError data: {3}\nCall Stack: \n{4}'.
                             format(error.status_code,
                                    error.error_text,
                                    error.error_code,
                                    json.dumps(error.error_data, indent=4, separators=(',', ': ')),
                                    traceback.format_exc()))
            return error.response()
        except:
            detailed_error = 'Unexpected error somewhere inside:\n' + traceback.format_exc()
            __django__.error(detailed_error)

            if not settings.DEBUG:
                detailed_error = 'Unexpected error somewhere inside'

            return Response({
                'resultCode': status.HTTP_500_INTERNAL_SERVER_ERROR,
                'errorText': detailed_error
            }, status=status.HTTP_500_INTERNAL_SERVER_ERROR)
    return handler
