import logging
import json
import pystache

from django.core.management.base import BaseCommand
from cms.forms import convert_meta_to_description
from cms.models import DataStructure

logger = logging.getLogger(__name__)
ATTRIBUTE_KEYS = ["optional", "advanced", "public"]


def get_data_type_nice_name(data_type):
    return DataStructure.DATA_TYPES[DataStructure.get_type_by_name(data_type)]


def set_data_structure_attributes(data_structure):
    attributes = []

    for key in ATTRIBUTE_KEYS:
        if key == "advanced" and data_structure.get(key):
            attributes.append(key.capitalize())
        if key == "optional" and not data_structure.get(key, False):
            attributes.append(f'<span style="color: red">{"required".capitalize()}</span>')
        if key == "public" and not data_structure.get(key, True):
            attributes.append("private".capitalize())

    return ", ".join([f"<b>{attribute}</b>" for attribute in attributes])


def process_data_structure(data_structure):
    data_structure["nice_meta"] = set_data_structure_attributes(data_structure)

    if "meta" in data_structure:
        data_structure["nice_meta"] += convert_meta_to_description(data_structure["meta"])

        if data_structure["meta"].get("multi", False):
            data_structure["nice_meta"] += f"<br><b>Multi-select</b>"

        if "options" in data_structure["meta"]:
            data_structure["nice_meta"] += f'<br>Options:<ul style="list-style: none"><li>' \
                f'{"</li><li>".join(data_structure["meta"]["options"])}</li></ul>'

        if data_structure["nice_meta"].find('<br>') == 0:
            data_structure["nice_meta"] = data_structure["nice_meta"].replace("<br>", "", 1)

    if "value" in data_structure and type(data_structure["value"]) in [list, dict]:
        data_structure["value"] = f'<pre>{json.dumps(data_structure["value"], indent=4)}</pre>'
    data_structure["type"] = get_data_type_nice_name(data_structure["type"])

    data_structure["description"] = data_structure["description"].replace("<br><br><br>", "")


def process_cms_structure_json():
    with open("cms/product_structure_template.html.mustache", "r") as f:
        mustache_template = f.read()

    with open("cms/cms_structure.json", "r") as f:
        product_types = json.load(f)
        for product_type in product_types:
            for context in product_type["contexts"]:
                for idx, data_structure in enumerate(context["values"]):
                    process_data_structure(data_structure)
                    data_structure['index'] = idx + 1

            with open(f"cms/{product_type['type']}.html", "w+") as out_file:
                out_file.write(pystache.render(mustache_template, product_type))


class Command(BaseCommand):
    help = "Generates html files for product types in cms_structure.json"

    def handle(self, *args, **options):
        process_cms_structure_json()
