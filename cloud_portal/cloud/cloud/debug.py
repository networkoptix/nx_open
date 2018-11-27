import logging
import time

logger = logging.getLogger(__name__)


def timer(func):

    def exec_foo(*args, **kwargs):
        start = time.time()
        func(*args, **kwargs)
        end = time.time()

        logger.info("{} elapsed time: {}".format(func.__name__, end - start))

    return exec_foo
