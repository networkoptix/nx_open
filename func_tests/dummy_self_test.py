import logging
from pprint import pformat

import pytest


@pytest.mark.self
@pytest.mark.debug
def test_command_line_args(request):
    logging.info("Options:\n%s", pformat(request.config.option.__dict__))


@pytest.mark.self
@pytest.mark.debug
def test_empty():
    pass
