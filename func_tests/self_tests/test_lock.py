import logging
from multiprocessing import Condition, Process
from threading import Thread, current_thread
from time import sleep

import pytest

from framework.os_access.exceptions import AlreadyAcquired
from framework.os_access.local_shell import local_shell
from framework.os_access.posix_access import portalocker_lock_acquired

_logger = logging.getLogger(__name__)
_default_timeout_sec = 10


@pytest.fixture(params=['shell_flock', 'portalocker'])
def lock(request, node_dir):
    path = node_dir / 'shell_flock.lock'
    return {
        'shell_flock':
            lambda timeout_sec=_default_timeout_sec:
            local_shell.lock_acquired(path, timeout_sec=timeout_sec),
        'portalocker':
            lambda timeout_sec=_default_timeout_sec:
            portalocker_lock_acquired(path, timeout_sec=timeout_sec),
        }[request.param]


def test_already_acquired_timed_out(lock):
    with lock():
        with pytest.raises(AlreadyAcquired):
            with lock(timeout_sec=2):
                pass


def test_already_acquired_wait_successfully(lock):
    exceptions = []

    def acquire_wait_release():
        try:
            with lock(timeout_sec=2):
                sleep(1)
        except Exception as e:
            _logger.exception("Exception in %r.", current_thread())
            exceptions.append(e)

    threads = [
        Thread(target=acquire_wait_release),
        Thread(target=acquire_wait_release),
        ]
    for thread in threads:
        thread.start()
    for thread in threads:
        thread.join(timeout=5)
    for exception in exceptions:
        raise exception


def _hold_lock(lock, cv):
    cv.acquire()
    with lock():
        cv.notify()
        cv.wait()


def test_cleanup_on_process_termination(lock):
    cv = Condition()
    cv.acquire()
    p = Process(target=_hold_lock, args=(lock, cv))
    p.start()
    cv.wait()
    p.terminate()
    with lock(timeout_sec=1):
        pass
