from ...models import Product, Language, Customization
import os
import codecs
import json
from django.core.management.base import BaseCommand


STATIC_DIR = 'static/'


def find_or_add_product(name):
    if Product.objects.filter(name=name).exists():
        return Product.objects.get(name=name)
    product = Product(name=name)
    product.save()
    return product


def find_or_add_customization(item, default_language):
    if Customization.objects.filter(name=item).exists():
        return Customization.objects.get(name=item)
    custom = Customization(name=item, default_language_id=default_language.id)
    custom.save()
    return custom


def find_or_add_language(name, code):
    if Language.objects.filter(code=code).exists():
        return Language.objects.get(code=code)
    lang = Language(name=name, code=code)
    lang.save()
    return lang


def find_or_add_language_to_customization(language, customization):
    customization.languages.add(language)
    customization.save()


# run read structure
def init_cms_db():
    find_or_add_product('cloud_portal')

    # read customizations
    for custom in os.listdir(STATIC_DIR):
        if custom in ('common', '_source'):
            continue
        if os.path.isdir(os.path.join(STATIC_DIR, custom)):
            # read languages and languages in customizations
            language_json_filename = os.path.join(
                STATIC_DIR, custom, 'source/static/languages.json')
            with codecs.open(language_json_filename, 'r', 'utf-8') as file_descriptor:
                data = json.load(file_descriptor)

                for lang in data:
                    language = find_or_add_language(
                        lang["name"], lang["language"])
                    customization = find_or_add_customization(custom, language)
                    find_or_add_language_to_customization(
                        language, customization)


class Command(BaseCommand):
    help = 'Creates initial records for CMS in the\
     database (customizations, languages, products)'

    def handle(self, *args, **options):
        init_cms_db()
        self.stdout.write(self.style.SUCCESS(
            'Successfully initiated database records for CMS'))
