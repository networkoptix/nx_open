from django.http import HttpResponse
from django.utils.log import AdminEmailHandler
import md5, traceback
from rest_framework import status

import logging
logger = logging.getLogger(__name__)


class LimitAdminEmailHandler(AdminEmailHandler):
    PERIOD_LENGTH_IN_SECONDS = 60*10  # 10 minutes
    MAX_EMAILS_IN_PERIOD = 1
    KEY_LENGTH = 150
    COUNTER_CACHE_KEY = "email_admins_counter_"

    def increment_counter(self, record):
        key_postfix = record.message[:self.KEY_LENGTH]
        key_postfix = md5.md5(key_postfix).hexdigest()
        from django.core.cache import cache
        try:
            cache.incr(self.COUNTER_CACHE_KEY + key_postfix)
        except ValueError:
            cache.set(self.COUNTER_CACHE_KEY + key_postfix, 1, self.PERIOD_LENGTH_IN_SECONDS)
        return cache.get(self.COUNTER_CACHE_KEY + key_postfix)

    def emit(self, record):
        try:
            counter = self.increment_counter(record)
        except Exception:
            print (traceback.format_exc())
        else:
            if counter > self.MAX_EMAILS_IN_PERIOD:
                return
        super(LimitAdminEmailHandler, self).emit(record)


class CatchExceptionMiddleware(object):
    def __init__(self, get_response):
        self.get_response = get_response

    def __call__(self, request):
        return self.get_response(request)

    def process_exception(self, request, exception):
        logging.info(request)
        logging.critical("{}: {}\nCall Stack:\n{}".format(exception.__class__.__name__,
                                                          exception.message,
                                                          traceback.format_exc().replace("Traceback", "")))
        return HttpResponse("Error with request", status=status.HTTP_500_INTERNAL_SERVER_ERROR)
