from django.core.management.base import BaseCommand
from ...controllers import filldata, structure
from ...models import Customization, Language, Product, ProductType, get_cloud_portal_product
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
            customization = Customization.objects.filter(name=options['customization'])
            if not customization.exists():
                en_us = Language.objects.get(code="en_US")
                customization = Customization(name=options['customizations'], default_language=en_us)
                customization.save()
                customization.languages = [en_us]
                customization.save()

            if not Product.objects.filter(customizations__in=[customization.first()],
                                          product_type__type=ProductType.PRODUCT_TYPES.cloud_portal).exists():
                product = structure.find_or_add_product('Cloud Portal', customization.first())
            else:
                product = get_cloud_portal_product(options['customization'])
            filldata.init_skin(product, options['preview'])
            self.stdout.write(self.style.SUCCESS(
                'Initiated static content for ' + product.__str__()))

        self.stdout.write(self.style.SUCCESS('Successfully initiated static content'))
