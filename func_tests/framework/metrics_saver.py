import datetime
import logging

log = logging.getLogger(__name__)


class MetricsSaver(object):

    def __init__(self, db_capture_repository, current_test_run):
        self._db_capture_repository = db_capture_repository  # None if junk shop plugin is not installed
        self._current_test_run = current_test_run

    def save(self, metric_name, metric_value):
        metric_value_str = self._value2str(metric_value)
        log.info('Metric: %s = %r / %r', metric_name, metric_value, metric_value_str)
        if self._db_capture_repository:
            self._db_capture_repository.add_metric_with_session(
                self._current_test_run, metric_name, metric_value_str)

    def _value2str(self, value):
        if isinstance(value, (float, int)):
            return value
        if isinstance(value, datetime.timedelta):
            return value.total_seconds()
        assert False, 'Unsupported metric value type: %s' % type(value)
