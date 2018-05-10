from django.core.management.base import BaseCommand

from django_celery_results.models import TaskResult

from ...models import *
from datetime import datetime, timedelta


class Command(BaseCommand):
    help = 'Cleans out django tasks results and messages older than {} days'\
        .format(settings.CLEAN_TASKS_AND_MSGS_OLDER_THAN_X_DAYS)

    def handle(self, *args, **options):
        cutoff_date = datetime.now() - timedelta(days=settings.CLEAN_TASKS_AND_MSGS_OLDER_THAN_X_DAYS)
        TaskResult.objects.filter(date_done__lt=cutoff_date).delete()
        Message.objects.filter(send_date__lt=cutoff_date).delete()
        self.stdout.write(self.style.SUCCESS('Successfully deleted task results and messages older than {} days'\
                                             .format(settings.CLEAN_TASKS_AND_MSGS_OLDER_THAN_X_DAYS)))