from django.core.management.base import BaseCommand
from ...controllers import filldata
from ...models import Customization, get_cloud_portal_product
from cloud import settings


class Command(BaseCommand):
    help = 'Fills initial data from CMS database to static files'

    def add_arguments(self, parser):
        parser.add_argument('customization', nargs='?', default=settings.CUSTOMIZATION)

    def handle(self, *args, **options):
        if 'customization' not in options:
            filldata.init_skin(settings.CUSTOMIZATION)
            self.stdout.write(self.style.SUCCESS(
                'Initiated static content for ' + settings.CUSTOMIZATION))
        elif options['customization'] == 'all':
            for custom in Customization.objects.all():
                product_name = get_cloud_portal_product(custom.name)
                filldata.init_skin(custom.name, product_name)
                self.stdout.write(self.style.SUCCESS('Initiated static content for ' + custom.name))
        else:
            product_name = get_cloud_portal_product(options['customization'])
            filldata.init_skin(options['customization'], product_name)
            self.stdout.write(self.style.SUCCESS(
                'Initiated static content for ' + options['customization']))

        self.stdout.write(self.style.SUCCESS('Successfully initiated static content'))
