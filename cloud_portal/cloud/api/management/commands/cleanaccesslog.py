from django.core.management.base import BaseCommand

from ...models import *
from datetime import datetime, timedelta
from cloud import settings


class Command(BaseCommand):
    help = 'Cleans out access log older than {} days'\
        .format(settings.CLEAR_HISTORY_RECORDS_OLDER_THAN_X_DAYS)

    def handle(self, *args, **options):
        cutoff_date = datetime.now() - timedelta(days=settings.CLEAR_HISTORY_RECORDS_OLDER_THAN_X_DAYS)
        AccountLoginHistory.objects.filter(date__lt=cutoff_date).delete()
        self.stdout.write(self.style.SUCCESS('Successfully deleted task results and messages older than {} days'\
                                             .format(settings.CLEAR_HISTORY_RECORDS_OLDER_THAN_X_DAYS)))
