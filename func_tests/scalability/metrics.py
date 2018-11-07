from collections import namedtuple
import logging
import threading

from framework.threaded import ThreadSafeCounter


_logger = logging.getLogger(__name__)


Metrics = namedtuple('Metrics', [
    'posted_transaction_count',
    'post_transaction_duration',
    'transaction_delay',
    'stale_time',
    ])


class MetricTriplet(namedtuple('_MetricTriplet', 'sum average max')):

    @classmethod
    def from_list(cls, item_list):
        items_sum = sum(item_list)
        if item_list:
            return cls(
                sum=items_sum,
                average=sum(item_list) / len(item_list),
                max=max(item_list),
                )
        else:
            return cls(items_sum, 0, 0)


class MetricsCollector(object):

    def __init__(self):
        self._lock = threading.Lock()
        self._posted_transaction_counter = 0
        self._transaction_delay_list = []
        self._post_duration_list = []
        self._stale_time_list = []
        self.socket_reopened_counter = ThreadSafeCounter()

    def incr_posted_transaction_counter(self):
        with self._lock:
            self._posted_transaction_counter += 1

    def add_post_duration(self, duration_sec):
        with self._lock:
            self._post_duration_list.append(duration_sec)

    def add_transaction_delay(self, delay):
        with self._lock:
            self._transaction_delay_list.append(delay)

    def add_stale_time(self, stale_time):
        with self._lock:
            self._stale_time_list.append(stale_time)

    def get_and_reset(self):
        with self._lock:
            metrics = Metrics(
                posted_transaction_count=self._posted_transaction_counter,
                post_transaction_duration=MetricTriplet.from_list(self._post_duration_list),
                transaction_delay=MetricTriplet.from_list(
                    [d.total_seconds() for d in self._transaction_delay_list]),
                stale_time=MetricTriplet.from_list(self._stale_time_list),
                )
            self._posted_transaction_counter = 0
            self._post_duration_list = []
            self._transaction_delay_list = []
            self._stale_time_list = []
            return metrics

    def report(self, required_rate=None, duration=None):
        metrics = self.get_and_reset()
        if duration:
            actual_rate = metrics.posted_transaction_count / duration.total_seconds()
        else:
            actual_rate = 0
        _logger.info('\n'
            + 'Issued {} transactions in {}\n'.format(metrics.posted_transaction_count, duration or '-')
            + 'transaction rate actual/required: {}/{}\n'.format(actual_rate, required_rate or '-')
            + 'transaction post duration sum/average/max: {:.2f}/{:.2f}/{:.2f} sec\n'.format(
                *metrics.post_transaction_duration)
            + 'transaction post stalled sum/average/max: {:.3f}/{:.3f}/{:.3f} sec\n'.format(
                *metrics.stale_time)
            + 'transaction delay average/max: {:.2f}/{:.2f} sec\n'.format(
                metrics.transaction_delay.average, metrics.transaction_delay.max)
            )
