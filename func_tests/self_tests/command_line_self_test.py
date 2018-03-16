import logging
from pprint import pformat

import pytest


@pytest.mark.self
def test_command_line(request):
    logging.info("Options:\n%s", pformat(request.config.option.__dict__))


