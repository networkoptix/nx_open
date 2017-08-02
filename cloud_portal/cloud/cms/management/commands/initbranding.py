import xml.etree.ElementTree as eTree
from django.core.management.base import BaseCommand
from ...models import Product, Language, Customization, DataStructure, Context, DataRecord
import os


def read_branding(customization_name):
    branding_file = os.path.join('static', customization_name, 'branding.ts')
    tree = eTree.parse(branding_file)
    root = tree.getroot()
    branding_messages = []
    for context in root.iter('context'):
        for message in context.iter('message'):
            source = message.find('source').text
            translation = message.find('translation').text
            branding_messages.append((source, translation))
    return branding_messages


class Command(BaseCommand):
    help = 'Creates initial records for CMS in the database\
     (customizations, languages, products)'

    def handle(self, *args, **options):
        for custom in Customization.objects.all():
            branding_context = Context.objects.get(name='branding')

            try:
                branding_messages = read_branding(custom.name)
            except IOError:
                continue

            for (key, value) in branding_messages:
                data_structure = DataStructure.objects.get(
                    name=key, context_id=branding_context.id)
                data_record = DataRecord.objects\
                    .filter(data_structure_id=data_structure.id,
                            customization_id=custom.id)
                if data_record.exists():  # data record exists - ignore
                    continue
                data_record = DataRecord(data_structure_id=data_structure.id,
                                         customization_id=custom.id,
                                         language=custom.default_language,
                                         value=value)
                data_record.save()  # write to database
        self.stdout.write(self.style.SUCCESS(
            'Successfully initiated database records for CMS'))
