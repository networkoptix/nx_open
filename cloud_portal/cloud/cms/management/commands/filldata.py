from django.core.management.base import BaseCommand
from ...controllers import filldata
from ...models import Customization
from cloud import settings


class Command(BaseCommand):
    help = 'Fills initial data from CMS database to static files'

    def add_arguments(self, parser):
        parser.add_argument('customization', nargs='?', default=settings.CUSTOMIZATION)

    def handle(self, *args, **options):

        # for custom in Customization.objects.all():
        #    filldata.fill_content(customization_name=custom.name, product='cloud_portal', preview=False)
        self.stdout.write(self.style.SUCCESS(
            'Successfully initiated static content'))

        if 'customization' not in options:
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
            self.stdout.write(self.style.SUCCESS(
                'Initiated static content for ' + settings.CUSTOMIZATION))
        elif options['customization'] == 'all':
            for custom in Customization.objects.all():
                filldata.fill_content(customization_name=custom.name,
                                      product='cloud_portal',
                                      preview=False,
                                      incremental=False)
                filldata.fill_content(customization_name=custom.name,
                                      product='cloud_portal',
                                      preview=True,
                                      incremental=False)
                self.stdout.write(self.style.SUCCESS('Initiated static content for ' + custom.name))
        else:
            filldata.fill_content(
                customization_name=options['customization'],
                product='cloud_portal',
                preview=False,
                incremental=False)
            filldata.fill_content(
                customization_name=options['customization'],
                product='cloud_portal',
                preview=True,
                incremental=False)
            self.stdout.write(self.style.SUCCESS(
                'Initiated static content for ' + options['customization']))

        self.stdout.write(self.style.SUCCESS('Successfully initiated static content'))
