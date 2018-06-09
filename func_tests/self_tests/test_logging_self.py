import logging

import pytest


@pytest.mark.self
def test_logging(work_dir):
    _logger = logging.getLogger('test_logger')
    _logger.info('Test info log entry')
    assert work_dir.joinpath('info.log').exists()
    _logger.debug('Test debug log entry')
    assert work_dir.joinpath('debug.log').exists()
