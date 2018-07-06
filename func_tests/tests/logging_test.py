from collections import namedtuple

import pytest

LogLevel = namedtuple('LogLevel', ['api_request', 'log_file'])

LOG_LEVELS = {
    'debug2': LogLevel('DEBUG2', 'VERBOSE'),
    'verbose': LogLevel('DEBUG2', 'VERBOSE'),
    'debug': LogLevel('DEBUG', 'DEBUG'),
    'info': LogLevel('INFO', 'INFO'),
    'warning': LogLevel('WARNING', 'WARNING'),
    'error': LogLevel('ERROR', 'ERROR'),
    'always': LogLevel('ALWAYS', 'ALWAYS'),
    }


@pytest.mark.parametrize('level', LOG_LEVELS.keys())
def test_set_level_in_configuration(running_linux_mediaserver, level):
    running_linux_mediaserver.stop()
    running_linux_mediaserver.installation.update_mediaserver_conf({'logLevel': level})
    running_linux_mediaserver.start()
    assert running_linux_mediaserver.api.get('api/logLevel', params={'id': 0}) == LOG_LEVELS[level].api_request
    assert not running_linux_mediaserver.installation.list_core_dumps()
