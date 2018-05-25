import logging

import pytest


@pytest.mark.self
def test_logging(work_dir):
    logger = logging.getLogger('test_logger')
    logger.info('Test info log entry')
    assert work_dir.joinpath('info.log').exists()
    logger.debug('Test debug log entry')
    assert work_dir.joinpath('debug.log').exists()
