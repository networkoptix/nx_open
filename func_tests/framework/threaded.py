from threading import Event, Thread

from framework.utils import with_traceback


class ThreadedCall(object):
    def __init__(self, call, terminate, timeout_sec=10):
        self._terminate = terminate
        self._timeout_sec = timeout_sec
        self._exception_event = Event()
        self.thread = Thread(target=with_traceback(call, self._exception_event))

    def __enter__(self):
        self.thread.start()

    def __exit__(self, exc_type, exc_val, exc_tb):
        self._terminate()
        self.thread.join(timeout=self._timeout_sec)
        if self._exception_event.is_set():
            raise RuntimeError('Exception is raised in ThreadedCall; check logs.')

    @classmethod
    def periodic(cls, iteration_call, sleep_between_sec=0, terminate_timeout_sec=10):
        stop_event = Event()

        def call():
            while not stop_event.is_set():
                iteration_call()
                stop_event.wait(sleep_between_sec)

        return cls(call, stop_event.set, timeout_sec=terminate_timeout_sec)
