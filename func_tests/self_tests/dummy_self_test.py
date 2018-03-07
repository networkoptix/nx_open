import logging
from pprint import pformat
from time import sleep

import multiprocessing

import portalocker
import pytest
from pathlib2 import Path


@pytest.mark.self
@pytest.mark.debug
def test_command_line_args(request):
    logging.info("Options:\n%s", pformat(request.config.option.__dict__))


@pytest.mark.self
@pytest.mark.debug
def test_empty():
    pass


@pytest.mark.self
@pytest.mark.debug
def test_clean_vm_pools(vm_pools):
    vm_pools['linux'].clean()


@pytest.mark.self
@pytest.mark.debug
def test_destroy_vm_pools(vm_pools):
    vm_pools['linux'].destroy()


path = Path('/', 'tmp', 'func_tests-virtualbox_vms')


def lock(seconds, i):
    with portalocker.Lock(str(path), mode='a+') as f:
        print(i, 'open for write')
        print(f.read())
        f.seek(0)
        f.truncate()
        f.write('oi' + str(i))
        sleep(seconds)
        print(i, 'close for write')


@pytest.mark.self
@pytest.mark.debug
def test_file_lock():
    multiprocessing.Process(target=lock, args=(0.1, 1)).start()
    multiprocessing.Process(target=lock, args=(0.1, 2)).start()
    multiprocessing.Process(target=lock, args=(0.1, 3)).start()


if __name__ == '__main__':
    test_file_lock()
