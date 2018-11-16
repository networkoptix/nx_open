import logging

from threading import Event, Lock, Thread

from framework.utils import description_from_func, with_traceback

_logger = logging.getLogger(__name__)


class ThreadedCall(object):
    def __init__(self, call, terminate, join_timeout_sec=10, description=None, exception_event=None, logger=_logger):
        self._terminate = terminate
        self._join_timeout_sec = join_timeout_sec
        self._description = description
        self._logger = logger
        self._exception_event = exception_event or Event()
        self.thread = Thread(target=with_traceback(call, self._exception_event))

    def __enter__(self):
        self.thread.start()

    def __exit__(self, exc_type, exc_val, exc_tb):
        self._logger.info('Stopping %s thread:', self._description)
        self._terminate()
        self.thread.join(timeout=self._join_timeout_sec)
        self._logger.info('Stopping %s thread: done; thread is joined', self._description)
        if self._exception_event.is_set():
            raise RuntimeError('Exception is raised in ThreadedCall; check logs.')

    @classmethod
    def periodic(
            cls,
            iteration_call,
            sleep_between_sec=0,
            join_timeout_sec=10,
            description=None,
            stop_event=None,
            exception_event=None,
            logger=_logger,
            ):
        if description is None:
            description = description_from_func(iteration_call)
        stop_event = stop_event or Event()

        def call():
            logger.info('Thread %s is started', description)
            try:
                while not stop_event.is_set():
                    iteration_call()
                    stop_event.wait(sleep_between_sec)
            finally:
                logger.info('Thread %s is finished', description)

        return cls(call, stop_event.set, join_timeout_sec, description, exception_event, logger)


class ThreadSafeCounter(object):

    def __init__(self):
        self._counter = 0
        self._lock = Lock()

    def incr(self):
        with self._lock:
            self._counter += 1

    def get(self):
        with self._lock:
            return self._counter

    def get_and_reset(self):
        with self._lock:
            count = self._counter
            self._counter = 0
            return count
