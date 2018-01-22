from requests.exceptions import SSLError


def is_permanent(error):
    if isinstance(error, SSLError):
        return True
    return False
