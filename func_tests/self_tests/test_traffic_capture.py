import time

from framework.os_access.os_access_interface import OSAccess
from framework.os_access.path import copy_file


def test_capture_file_exists(one_vm, node_dir):
    os_access = one_vm.os_access  # type: OSAccess
    with os_access.traffic_capture.capturing() as capture_path:
        time.sleep(2)
    assert capture_path.exists()
    copy_file(capture_path, node_dir / 'capture.cap')
