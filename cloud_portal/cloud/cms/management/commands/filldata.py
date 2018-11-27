from django.core.management.base import BaseCommand
from ...controllers import filldata
from ...models import Customization, get_cloud_portal_product
from cloud import settings
from cloud.debug import timer


class Command(BaseCommand):
    help = 'Fills initial data from CMS database to static files'

    def add_arguments(self, parser):
        parser.add_argument('--customization', nargs='?', default=settings.CUSTOMIZATION, type=str)
        parser.add_argument('--preview', nargs='?', default=False, type=bool)

    @timer
    def handle(self, *args, **options):
        if options['customization'] == 'all':
            for custom in Customization.objects.all():
                product = get_cloud_portal_product(custom.name)
                filldata.init_skin(product, options['preview'])
                self.stdout.write(self.style.SUCCESS('Initiated static content for ' + product.__str__()))
        else:
            product = get_cloud_portal_product(options['customization'])
            filldata.init_skin(product, options['preview'])
            self.stdout.write(self.style.SUCCESS(
                'Initiated static content for ' + product.__str__()))

        self.stdout.write(self.style.SUCCESS('Successfully initiated static content'))
