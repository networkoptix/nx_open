from django.core.management.base import BaseCommand
from ...controllers import filldata, structure
from ...models import Customization
from cloud import settings


class Command(BaseCommand):
    help = 'Fills initial data from CMS database to static files'

    def add_arguments(self, parser):
        parser.add_argument('customization', nargs='?', default=settings.CUSTOMIZATION)
        parser.add_argument('product', nargs='?', default=settings.PRIMARY_PRODUCT)

    def handle(self, *args, **options):
        if 'customization' not in options:
            filldata.init_skin(settings.CUSTOMIZATION)
            self.stdout.write(self.style.SUCCESS(
                'Initiated static content for ' + settings.CUSTOMIZATION))
        elif options['customization'] == 'all':
            for custom in Customization.objects.all():
                product_name = 'cloud_portal_{}'.format(custom.name)
                structure.find_or_add_product(product_name, True)
                filldata.init_skin(custom.name, product_name)
                self.stdout.write(self.style.SUCCESS('Initiated static content for ' + custom.name))
        else:
            product = structure.find_or_add_product(options['product'], True)
            product.customizations = [Customization.objects.get(name=options['customization'])]
            product.save()
            filldata.init_skin(options['customization'], options['product'])
            self.stdout.write(self.style.SUCCESS(
                'Initiated static content for ' + options['customization']))

        self.stdout.write(self.style.SUCCESS('Successfully initiated static content'))
