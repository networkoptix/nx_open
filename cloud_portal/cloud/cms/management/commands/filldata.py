from django.core.management.base import BaseCommand
from ...controllers import filldata
from ...models import Customization
from cloud import settings


class Command(BaseCommand):
    help = 'Fills initial data from CMS database to static files'

    def handle(self, *args, **options):
        filldata.fill_content(
            customization_name=settings.CUSTOMIZATION,
            product='cloud_portal',
            preview=False,
            incremental=False)
        filldata.fill_content(
            customization_name=settings.CUSTOMIZATION,
            product='cloud_portal',
            preview=True,
            incremental=False)
        # for custom in Customization.objects.all():
        #    filldata.fill_content(customization_name=custom.name, product='cloud_portal', preview=False)
        self.stdout.write(self.style.SUCCESS(
            'Successfully initiated static content'))
