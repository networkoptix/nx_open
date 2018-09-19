from django.core.management.base import BaseCommand
from ...controllers import filldata
from ...models import Customization, get_cloud_portal_product
from cloud import settings


class Command(BaseCommand):
    help = 'Fills initial data from CMS database to static files'

    def add_arguments(self, parser):
        parser.add_argument('customization', nargs='?', default=settings.CUSTOMIZATION)

    def handle(self, *args, **options):
        if options['customization'] == 'all':
            for custom in Customization.objects.all():
                filldata.init_skin(get_cloud_portal_product(custom.name), custom.name)
                self.stdout.write(self.style.SUCCESS('Initiated static content for ' + custom.name))
        else:
            customization_name = options['customization']
            filldata.init_skin(get_cloud_portal_product(customization_name), customization_name)
            self.stdout.write(self.style.SUCCESS(
                'Initiated static content for ' + options['customization']))

        self.stdout.write(self.style.SUCCESS('Successfully initiated static content'))
