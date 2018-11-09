from collections import namedtuple
import logging
import re

from framework.mediaserver_api import DEFAULT_API_PASSWORD, DEFAULT_API_USER, MediaserverApi


_logger = logging.getLogger(__name__)


_ExistingMediaserver = namedtuple('_ExistingMediaserver', 'name port api')


def create_existing_server_list(url_list, password=None):

    def url_to_name(url):
        name, _ = re.subn('[:.]', '-', url)
        return 'server-{}'.format(name)

    server_list = [
        _ExistingMediaserver(
            name=url_to_name(url),
            port=url.split(':')[1],
            api=MediaserverApi('{}:{}@{}'.format(
                DEFAULT_API_USER, password or DEFAULT_API_PASSWORD, url)),
            )
        for url in url_list or []
        ]
    for server in server_list:
        _logger.info('Server: %s', server)
    return server_list


def address_and_count_to_server_list(server_address_list, server_count, base_port, is_lws):
    if is_lws:
        return create_existing_server_list(
            ['{}:{}'.format(server_address_list[-1], base_port)]
            + ['{}:{}'.format(server_address_list[0], 8001 + idx)
               for idx in range(server_count - 1)],
            password='admin')
    else:
        return create_existing_server_list([
            '{}:{}'.format(address, base_port + idx)
            for address in server_address_list
            for idx in range(server_count // len(server_address_list))
            ])
