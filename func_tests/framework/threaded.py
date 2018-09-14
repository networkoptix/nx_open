from threading import Thread


class ThreadedCall(object):
    def __init__(self, call, terminate, timeout_sec=10):
        self._terminate = terminate
        self._timeout_sec = timeout_sec
        self.thread = Thread(target=call)

    def __enter__(self):
        self.thread.start()

    def __exit__(self, exc_type, exc_val, exc_tb):
        self._terminate()
        self.thread.join(timeout=self._timeout_sec)
