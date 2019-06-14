import logging
import time
from django.core.management.base import BaseCommand
from ...controllers import filldata, structure
from ...models import Customization, Language, Product, ProductType, get_cloud_portal_product
from cloud import settings
from cloud.debug import timer

logger = logging.getLogger(__name__)


class Command(BaseCommand):
    help = 'Fills initial data from CMS database to static files'

    def add_arguments(self, parser):
        parser.add_argument('--customization', nargs='?', default=settings.CUSTOMIZATION, type=str)
        parser.add_argument('--preview', nargs='?', default=False, type=bool)

    @timer
    def handle(self, *args, **options):
        customization = Customization.objects.filter(name=options['customization']).first()
        if not customization:
            logger.error('Customization {0} was automatically generated. Ask Boris to configure cloud for {0}.'
                         .format(options['customization']))
            en_us = Language.objects.get(code="en_US")
            customization = Customization(name=options['customization'], default_language=en_us)
            customization.save()
            customization.languages.add(en_us)
            customization.save()

        if not Product.objects.filter(customizations__in=[customization],
                                      product_type__type=ProductType.PRODUCT_TYPES.cloud_portal).exists():
            product = structure.find_or_add_product('Cloud Portal', customization, 'cloud_portal', '')
        else:
            product = get_cloud_portal_product(options['customization'])

        for i in range(settings.FILLDATA_TRIES):
            result = filldata.init_skin(product, options['preview'], workers=1)
            if result:
                break

            warning_msg = f"Filldata Failed. Retrying in {settings.FILLDATA_TIMEOUT} seconds"
            logger.warning(warning_msg)
            self.stdout.write(self.style.WARNING(warning_msg))
            time.sleep(settings.FILLDATA_TIMEOUT)

        else:
            error_msg = f"Filldata failed after running {settings.FILLDATA_TRIES} time(s). " \
                f"Run forceupdate for {product.__str__()} to fix the problem."
            logger.critical(error_msg)
            self.stdout.write(self.style.ERROR(error_msg))
            return

        self.stdout.write(self.style.SUCCESS(f"Successfully initiated static content for {product.__str__()}"))
