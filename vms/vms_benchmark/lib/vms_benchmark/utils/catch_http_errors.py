import urllib.error
from vms_benchmark import exceptions


def catch_http_errors(action):
    def result_func(*args, **kwargs):
        try:
            return action(*args, **kwargs)
        except urllib.error.HTTPError as e:
            if e.code == 401 or e.code == 403:
                raise exceptions.ServerApiError(
                    'Server failed to execute API request: '
                    f'insufficient VMS user permissions (HTTP code {e.code}).',
                    original_exception=e
                )
            if e.code == 400 or e.code == 404 or e.code == 406:
                raise exceptions.ServerApiError(
                    'Server failed to execute API request: '
                    f'Server internal error (HTTP code {e.code}).',
                    original_exception=e
                )
            if 500 <= e.code <= 599:
                raise exceptions.ServerApiError(
                    'Server failed to execute API request: '
                    f'Server internal error (HTTP code {e.code}).',
                    original_exception=e
                )
            raise exceptions.ServerApiError(e)
    return result_func
