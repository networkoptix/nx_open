import os
import re
import xml.etree.ElementTree as eTree
import yaml
from os.path import join

import json
import errno
import yaml
import shutil
import codecs


def make_dir(filename):
    dirname = os.path.dirname(filename)
    print ("make dir " + dirname + " for " + filename)
    if not os.path.exists(dirname):
        try:
            os.makedirs(dirname)
        except OSError as exc:  # Guard against race condition
            if exc.errno != errno.EEXIST:
                raise


def save_content(filename, content):
    if filename:
        # proceed with branding
        make_dir(filename)
        with codecs.open(filename, "w", "utf-8") as file:
            file.write(content)
            file.close()


def generate_languages_files(languages):
    languages_json = []
    # Localize this language
    for lang in languages:
        language_json_filename = os.path.join("../../..", "translations", lang, 'language.json')
        with open(language_json_filename, 'r') as file_descriptor:
            data = json.load(file_descriptor)
            data["language"] = lang
            languages_json.append({
                "language": lang,
                "name": data["language_name"] if "language_name" in data else data["name"]
            })
    save_content('static/languages.json', json.dumps(languages_json, ensure_ascii=False))


# Read config - get languages there
config = yaml.safe_load(open('cloud_portal.yaml'))

# Iterate languages
if 'languages' not in config:
    raise 'No languages section in cloud_portal.yaml'

generate_languages_files(config['languages'])
