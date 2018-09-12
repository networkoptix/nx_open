import time

from framework.os_access.os_access_interface import OSAccess
from framework.os_access.path import copy_file


def test_capture_file_exists(one_vm, node_dir):
    os_access = one_vm.os_access  # type: OSAccess
    local_capture_file = node_dir / 'capture.cap'
    with os_access.traffic_capture.capturing(local_capture_file) as remote_capture_file:
        time.sleep(2)
    assert remote_capture_file.exists()
    assert local_capture_file.exists()
