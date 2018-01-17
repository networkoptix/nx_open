import base64
import json
import codecs
import os
import re
from zipfile import ZipFile
from ..models import Product, Context, DataStructure, Customization, DataRecord


def find_or_add_product(name):
    if Product.objects.filter(name=name).exists():
        return Product.objects.get(name=name)
    product = Product(name=name)
    product.save()
    return product


def find_or_add_context(context_name, old_name, product_id, has_language, is_global):
    if old_name and Context.objects.filter(name=old_name, product_id=product_id).exists():
        context = Context.objects.get(name=old_name, product_id=product_id)
        context.name = context_name
        context.save()
        return context

    if Context.objects.filter(name=context_name, product_id=product_id).exists():
        return Context.objects.get(name=context_name, product_id=product_id)

    context = Context(name=context_name, file_path=context_name,
                      product_id=product_id, translatable=has_language,
                      is_global=is_global)
    context.save()
    return context


def find_or_add_data_structure(name, old_name, context_id, has_language):
    if old_name and DataStructure.objects.filter(name=old_name, context_id=context_id).exists():
        record = DataStructure.objects.get(name=old_name, context_id=context_id)
        record.name = name
        record.save()
        return record

    if DataStructure.objects.filter(name=name, context_id=context_id).exists():
        return DataStructure.objects.get(name=name, context_id=context_id)
    data = DataStructure(name=name, context_id=context_id,
                         translatable=has_language, default=name)
    data.save()
    return data


def update_from_object(cms_structure):
    product_name = cms_structure['product']

    default_language = Customization.objects.get(name='default').default_language.code
    product_id = find_or_add_product(product_name).id
    order = 0

    for context_data in cms_structure['contexts']:
        has_language = context_data["translatable"]
        is_global = context_data["is_global"] if "is_global" in context_data else False
        old_name = context_data["old_name"] if "old_name" in context_data else None
        context = find_or_add_context(
            context_data["name"], old_name, product_id, has_language, is_global)
        if "description" in context_data:
            context.description = context_data["description"]
        if "file_path" in context_data:
            context.file_path = context_data["file_path"]
        if "url" in context_data:
            context.url = context_data["url"]
        context.is_global = is_global
        context.hidden = context_data["hidden"] if "hidden" in context_data else False
        context.save()

        for record in context_data["values"]:
            if not isinstance(record, dict):
                old_name = None
                description = None
                record_type = "text"
                meta = None
                advanced = False
                optional = False
                label = None
                value = None
                if len(record) == 3:
                    label, name, value = record
                if len(record) == 4:
                    label, name, value, description = record
            else:
                name = record['name']
                value = record['value'] if 'value' in record else ""
                label = record['label'] if 'label' in record else ""
                old_name = record['old_name'] if 'old_name' in record else None
                description = record['description'] if 'description' in record else None
                record_type = record['type'] if 'type' in record else None
                meta = record['meta'] if 'meta' in record else None
                advanced = record['advanced'] if 'advanced' in record else False
                optional = record['optional'] if 'optional' in record else False

            data_structure = find_or_add_data_structure(name, old_name, context.id, has_language)

            data_structure.order = order
            order += 1
            data_structure.label = label
            data_structure.advanced = advanced
            data_structure.optional = optional
            if description:
                data_structure.description = description
            if record_type:
                data_structure.type = DataStructure.get_type(record_type)

            if data_structure.type == DataStructure.DATA_TYPES.image:
                data_structure.translatable = "{{language}}" in name

                # this is used to convert source images into b64 strings
                file_path = os.path.join('static', '_source', 'blue', name)
                file_path = file_path.replace("{{language}}", default_language)
                try:
                    with open(file_path, 'r') as file:
                        value = base64.b64encode(file.read())
                except IOError:
                    pass

            data_structure.meta_settings = meta if meta else {}
            data_structure.default = value
            data_structure.save()


def read_structure_json(filename):
    with codecs.open(filename, 'r', 'utf-8') as file_descriptor:
        cms_structure = json.load(file_descriptor)
        update_from_object(cms_structure)


def process_zip(file_descriptor, user, update_structure, update_content):
    log_messages = []
    zip_file = ZipFile(file_descriptor)
    # zip_file.printdir()
    root = None

    if update_structure:
        name = next((name for name in zip_file.namelist() if name.endswith('structure.json')), None)
        if name:
            data = zip_file.read(name)
            cms_structure = json.loads(data)
            update_from_object(cms_structure)
            log_messages.append(('success', 'Updated from json using %s' % name))
        else:
            log_messages.append(('warning', 'Not found structure.json file'))

    for name in zip_file.namelist():
        # log_messages.append(('info', 'Processing %s' % name))
        if name.startswith('__') or name.endswith('structure.json'):
            # Ignore trash in archive from MACs or **structure.json files
            log_messages.append(('info', 'Ignored: %s' % name))
            continue

        if name.endswith('/'):
            if name.count('/') == 1:  # top level directories are customizations
                root = name
                customization_name = root.replace('/', '')
            continue  # not a file - ignore it

        short_name = name.replace(root, '')

        if short_name.startswith('help/'):  # Ignore help
            if short_name == 'help/':
                log_messages.append(('info', 'Ignored: %s (help directory is ignored)' % name))
            continue

        # try to find relevant context
        context = Context.objects.filter(file_path=short_name)
        if context.exists():
            try:
                file_content = zip_file.read(name).decode("utf-8")
            except UnicodeDecodeError:
                log_messages.append(('error', 'Ignored: %s (file is not UTF-encoded)' % name))
                continue

            context = context.first()
            if update_structure:
                if context.template != file_content:
                    context.template = file_content
                    context.save()
                    log_messages.append(('success', 'Updated template for context %s using %s' % (context.name, name)))
            if update_content:
                customization = Customization.objects.filter(name=customization_name)
                if not customization.exists():
                    log_messages.append(
                        ('warning', 'Ignored %s (customization %s not found)' % (name, customization_name)))
                    continue

                customization = customization.first()

                # try to parse datastructures from the file using template
                if not context.template:  # no template - nothing we can do
                    log_messages.append(('error', 'Ignored: %s (context has to template)' % name))
                    continue
                # here we have context.template and file_content - which are relatively close.
                # Ideally, the only difference is specific data values

                for structure in context.datastructure_set.all():
                    if structure.type in (DataStructure.DATA_TYPES.image, DataStructure.DATA_TYPES.file):
                        continue

                    # find a line in template which has structure.name in it
                    template_line = next((line for line in context.template.split("\n") if structure.name in line),
                                         None)
                    if not template_line:
                        log_messages.append(('warning', 'No line in template %s for data structure %s' %
                                             (short_name, structure.name)))
                        continue

                    replace_str = '(.*?)' if structure.type != structure.DATA_TYPES.html else '([.\s\S]*)'
                    # create regex using this line
                    template_line = re.escape(template_line)
                    escape_name = re.escape(structure.name)
                    template_line = template_line.replace(escape_name, replace_str)

                    if structure.type != structure.DATA_TYPES.html:
                        template_line += "$"

                    # try to parse file_content with regex
                    result = re.search(template_line, file_content)
                    if not result:
                        log_messages.append(('warning', 'No line in file %s for data structure %s' %
                                             (name, structure.name)))
                        continue

                    # if there is a value - compare it with latest draft
                    value = result.group(1)
                    current_value = structure.find_actual_value(customization)
                    if value == current_value:
                        log_messages.append(('warning', 'value %s not changed %s for data structure %s' %
                                             (value, name, structure.name)))
                        continue

                    # save if needed
                    record = DataRecord(data_structure=structure,
                                        customization=customization,
                                        value=value,
                                        created_by=user)
                    record.save()
                    log_messages.append(('success', 'Updated value for data structure %s using %s' %
                                         (structure.label, name)))
            continue

        # try to find relevant data structure and update its default (maybe)
        structure = DataStructure.objects.filter(name=short_name)
        if not structure.exists():
            log_messages.append(('warning', 'Ignored: %s (data structure %s does not exist)' % (name, short_name)))
            continue
        structure = structure.first()

        # if data structure is not FILE or IMAGE - print to log and ignore
        if structure.type not in (DataStructure.DATA_TYPES.image, DataStructure.DATA_TYPES.file):
            log_messages.append(('warning', 'Ignored: %s (data structure type is %s, not a %s or %s)' %
                                 (name, structure.type, DataStructure.DATA_TYPES.image, DataStructure.DATA_TYPES.file)))
            continue

        data = zip_file.read(name)
        data64 = base64.b64encode(data)

        if update_structure:
            # if set_defaults or data structure has no default value - save it
            if structure.default != data64:
                structure.default = data64
                log_messages.append(('success', 'Updated default for data structure %s using %s' %
                                     (structure.label, name)))
                structure.save()

        if update_content:
            customization = Customization.objects.filter(name=customization_name)
            if not customization.exists():
                log_messages.append(('warning', 'Ignored %s (customization %s not found)' % (name, customization_name)))
                continue
            customization = customization.first()
            # get latest value
            latest_value = structure.find_actual_value(customization)
            # check if file was changed
            if latest_value == data64:
                continue

            # add new dataRecrod
            record = DataRecord(
                data_structure=structure,
                customization=customization,
                value=data64,
                created_by=user
            )
            record.save()
            log_messages.append(('success', 'Updated value for data structure %s using %s' % (structure.label, name)))
    log_messages.append(('success', 'Finished'))
    return log_messages
