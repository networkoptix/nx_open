from django.core.management.base import BaseCommand, CommandError
from ...models import *


class Command(BaseCommand):
    help = 'Migrates old data records from original cloud_portal to new cloud_portals'

    def handle(self, *args, **options):
        try:
            original_cloud_portal = Product.objects.get(id=1)
        except Product.DoesNotExist:
            raise CommandError('Original Cloud Portal has been migrated already')

        new_clouds = Product.objects.filter(product_type__type=ProductType.PRODUCT_TYPES.cloud_portal).exclude(
            id=original_cloud_portal.id)
        original_records = DataRecord.objects.filter(data_structure__context__product__id=original_cloud_portal.id)

        for cloud in new_clouds:
            customization_records = original_records.filter(customization=cloud.customizations.first())

            for data_structure in DataStructure.objects.filter(context__product__id=cloud.id):

                for data_record in customization_records.filter(data_structure__name=data_structure.name):
                    data_record.data_structure_id = data_structure.id
                    data_record.save()
            self.stdout.write(
                self.style.SUCCESS('Successfully moved records for {}'.format(cloud.customizations.first().name)))

        self.stdout.write(self.style.SUCCESS('Successfully moved records to new products'))
