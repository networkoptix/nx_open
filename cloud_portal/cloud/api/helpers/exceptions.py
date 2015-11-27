from rest_framework.response import Response
from rest_framework import status
from django.conf import settings
import logging
import traceback

logger = logging.getLogger('django')


def api_success(data=None, status_code=status.HTTP_200_OK):
    if data is not None:
        return Response(data, status=status_code)
    return Response({
                'resultCode': 0
            }, status=status_code)


class APIException(Exception):
    error_text = 'Unexpected error'
    error_code = 500
    status_code = status.HTTP_500_INTERNAL_SERVER_ERROR

    def __init__(self, error_text, error_code=None, status_code=status.HTTP_500_INTERNAL_SERVER_ERROR):
        if error_code is None:
            error_code = status_code
        self.error_code = error_code
        self.error_text = error_text
        self.status_code = status_code

    def response(self):
        return Response({
                'resultCode': self.error_code,
                'errorText': self.error_text
            }, status=self.status_code)


class APIInternalException(APIException):
    # 500 error - internal server error
    def __init__(self, error_text, error_code=None):
        super(APIException, self).__init__(error_text, error_code=error_code,
                                           status_code=status.HTTP_500_INTERNAL_SERVER_ERROR)


class APIServiceException(APIException):
    # 503 error - service unavailable
    def __init__(self, error_text, error_code=None):
        super(APIException, self).__init__(error_text, error_code=error_code,
                                           status_code=status.HTTP_503_SERVICE_UNAVAILABLE)


class APINotFoundException(APIException):
    # 404 error - service unavailable
    def __init__(self, error_text, error_code=None):
        super(APIException, self).__init__(error_text, error_code=error_code, status_code=status.HTTP_404_NOT_FOUND)


class APINotAuthorisedException(APIException):
    # 401 error - service unavailable
    def __init__(self, error_text, error_code=None):
        super(APIException, self).__init__(error_text, error_code=error_code, status_code=status.HTTP_401_UNAUTHORIZED)


class APIRequestException(APIException):
    # 400 error - bad request
    def __init__(self, error_text, error_code=None):
        super(APIException, self).__init__(error_text, error_code=error_code, status_code=status.HTTP_400_BAD_REQUEST)


class APILogicException(APIException):
    # 200. but some error occurred
    def __init__(self, error_text, error_code=None):
        super(APIException, self).__init__(error_text, error_code=error_code, status_code=status.HTTP_200_OK)


def validate_response(func):
    def validate_error(response_data):
        if 'resultCode' not in response_data or 'errorText' not in response_data:
                raise APIInternalException('No valid error message from cloud_db')

    def validator(*args, **kwargs):
        response = func(*args, **kwargs)

        logger.debug(response.status_code)
        logger.debug(response.text)
        logger.debug('------------------------------------------------------------')

        if response.status_code == status.HTTP_204_NO_CONTENT:  # No content
            return None

        # 1. Validate JSON response. If not a json - raise exception
        # Treat JSON response problems as Internal Server Errors
        try:
            response_data = response.json()
        except ValueError:
            raise APIInternalException('No JSON data from cloud_db')

        # 2. Check 500 status  - raise APIInternalException
        if response.status_code == status.HTTP_500_INTERNAL_SERVER_ERROR:
            validate_error(response_data)
            raise APIInternalException(response_data['errorText'], response_data['resultCode'])

        # 3. Check 503 status - raise APIServiceException
        if response.status_code == status.HTTP_503_SERVICE_UNAVAILABLE:
            validate_error(response_data)
            raise APIServiceException(response_data['errorText'], response_data['resultCode'])

        # 4. Check 404 status - raise APINotFoundException
        if response.status_code == status.HTTP_404_NOT_FOUND:
            validate_error(response_data)
            raise APINotFoundException(response_data['errorText'], response_data['resultCode'])

        # 5. Check 401 status - raise APINotAuthorisedException
        if response.status_code == status.HTTP_401_UNAUTHORIZED:
            validate_error(response_data)
            raise APINotAuthorisedException(response_data['errorText'], response_data['resultCode'])

        # 6. Check 400 status - raise APIRequestException
        if response.status_code == status.HTTP_400_BAD_REQUEST:
            validate_error(response_data)
            raise APIRequestException(response_data['errorText'], response_data['resultCode'])

        # 7. Check error_code status - raise APILogicException
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
            if not isinstance(data, "Response"):
                return Response(data, status=status.HTTP_200_OK)
            return data
        except APIException as error:
            logger.error('APIException. \nStatus: ' + error.status_code +
                         '\nMessage:' + error.error_text +
                         '\nError code:' + error.error_code +
                         '\nCall Stack: \n' + traceback.format_exc())
            return status.response()
        except:
            detailed_error = 'Unexpected error somewhere inside:\n' + traceback.format_exc()
            logger.error(detailed_error)

            if not settings.DEBUG:
                detailed_error = 'Unexpected error somewhere inside'

            return Response({
                'resultCode': status.HTTP_500_INTERNAL_SERVER_ERROR,
                'errorText': detailed_error
            }, status=status.HTTP_500_INTERNAL_SERVER_ERROR)
        pass

    return handler
