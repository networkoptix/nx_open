import logging
import threading

from framework.waiting import Wait, wait_for_truthy

_logger = logging.getLogger(__name__)


def test_stop_timeout(two_merged_mediaservers):
    mediaserver = two_merged_mediaservers[0]
    service = mediaserver.installation.service
    exceptions = []
    timed_out = [False]

    def check_and_make_core_dump(status_before):
        acceptable_stop_time = 10
        wait = Wait("{} is stopped".format(service), timeout_sec=acceptable_stop_time)
        while True:
            status = service.status()
            process_is_running = status.pid is None  # TODO: Check process state directly.
            if process_is_running:
                _logger.info("Stopped.")
                break
            if not wait.again():
                if status.pid == status_before.pid:
                    _logger.info("Timed out.")
                    mediaserver.os_access.make_core_dump(status.pid)  # This makes process restart.
                    timed_out[0] = True
                else:
                    exceptions.append(RuntimeError("Server restarted but asked to stop"))
                break
            wait.sleep()

    if not service.status().is_running:
        service.start()
    wait_for_truthy(mediaserver.api.is_online)
    status_before = service.status()
    thread = threading.Thread(target=check_and_make_core_dump, args=(status_before,))
    thread.start()
    service.stop(timeout_sec=130)  # 120 sec is default timeout in service conf.
    thread.join()
    for exception in exceptions:
        raise exception
    assert not timed_out[0]
