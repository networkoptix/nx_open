from collections import namedtuple
import logging
import re

from framework.mediaserver_api import DEFAULT_API_PASSWORD, DEFAULT_API_USER, MediaserverApi


_logger = logging.getLogger(__name__)


_LocalMediaserver = namedtuple('_LocalMediaserver', 'name api')


def create_local_server_list(url_list, password=None):

    def url_to_name(url):
        name, _ = re.subn('[:.]', '-', url)
        return 'server-{}'.format(name)

    server_list = [
        _LocalMediaserver(
            name=url_to_name(url),
            api=MediaserverApi('{}:{}@{}'.format(
                DEFAULT_API_USER, password or DEFAULT_API_PASSWORD, url)),
            )
        for url in url_list or []
        ]
    for server in server_list:
        _logger.info('Server: %s', server)
    return server_list
