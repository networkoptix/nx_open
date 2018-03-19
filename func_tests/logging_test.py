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
def test_set_level_in_configuration(single_server, level):
    single_server.stop()
    single_server.installation.change_config(logLevel=level)
    single_server.start()
    assert single_server.rest_api.get('api/logLevel', params={'id': 0}) == LOG_LEVELS[level].api_request
