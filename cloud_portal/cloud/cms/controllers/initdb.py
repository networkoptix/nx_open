from ..models import Product, Language, Customization
import os
import codecs
import json


STATIC_DIR = 'static/'


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
    if Product.objects.exists():
        return

    # create product
    product = Product(name='cloud_portal')
    product.save()

    print STATIC_DIR
    # read customizations
    for custom in os.listdir(STATIC_DIR):
        print custom
        if custom == 'common'
            continue
        if os.path.isdir(os.path.join(STATIC_DIR, custom)):
            # read languages and languages in customizations
            language_json_filename = os.path.join(STATIC_DIR, custom, 'source/static/languages.json')
            with codecs.open(language_json_filename, 'r', 'utf-8') as file_descriptor:
                data = json.load(file_descriptor)

                for lang in data:
                    language = find_or_add_language(lang["name"], lang["language"])
                    customization = find_or_add_customization(custom, language)
                    find_or_add_language_to_customization(language, customization)
