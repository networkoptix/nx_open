from api.helpers.exceptions import get_client_ip, APINotAuthorisedException, ErrorCodes
from cloud import settings
import ipaddress


def check_ip(ip, list_name):
    white_list = settings.IP_WHITELISTS[list_name]
    ip_match = ip in white_list

    if not ip_match:
        for mask in white_list:
            network = ipaddress.ip_network(mask)
            if ipaddress.ip_address(ip) in network:
                ip_match = True
                break
    return ip_match


def ip_allow_only(list_name):
    def decorator(func):
        def handler(*args, **kwargs):
            request = args[0]
            ip = get_client_ip(request)
            ip_match = check_ip(ip, list_name)

            if not ip_match:
                raise APINotAuthorisedException('Your IP is not allowed for this action', ErrorCodes.forbidden)
            return func(*args, **kwargs)
        return handler
    return decorator
