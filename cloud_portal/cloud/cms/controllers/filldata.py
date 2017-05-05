from ..models import *
import os

# fill data from CMS (database) to sources


def process_context(context, language, customization):
    filename = context.file_path.replace("{{language}}", language.code)
    source_file = os.path.join('static', customization.name, 'source', filename)
    with open(source_file, 'r') as file:
        content = file.read()

    # for each structure record:
    for record in context.datastructure_set():
        # try to get translated content
        content_record = DataRecord.object.filter(language_id=language.id,
                                                  data_structure_id=record.id,
                                                  customization_id=customization.id)
        # if not - get default language
        if not content_record.exists():
            content_record = DataRecord.object.filter(language_id=customization.default_language_id,
                                                      data_structure_id=record.id,
                                                      customization_id=customization.id)

        if content_record.exists():
            content_value = content_record.value
        else:  # if no value - use default value from structure
            content_value = record.default

        # replace marker with value
        content = content.replace(record.name, content_value)

    # write content to target place
    target_file = os.path.join('static', customization, filename)
    with open(target_file, 'w') as file:
        file.write(content)


def fill_content(customization_name='default', product='cloud_portal'):
    product_id = Product.objects.get(name=product).id
    customization = Customization.objects.get(name=customization_name)

    # go context by context in product
    contexts = Context.objects.filter(product_id=product_id).all()
    for context in contexts:
        for language in customization.languages:
            # read template (translated)
            process_context(context, language, customization)
