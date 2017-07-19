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


def merge(source, destination):
    for key, value in source.items():
        if isinstance(value, dict):
            # get node or create one
            node = destination.setdefault(key, {})
            merge(value, node)
        else:
            destination[key] = value

    return destination


def merge_json(target_filename, source_filename, key=None):
    if not os.path.exists(source_filename) or not os.path.exists(target_filename):
        return
    with codecs.open(target_filename, 'r', 'utf-8') as file_descriptor:
        target_content = json.load(file_descriptor)
    with codecs.open(source_filename, 'r', 'utf-8') as file_descriptor:
        source_content = json.load(file_descriptor)

    if key is None:
        merge(source_content, target_content)
    else:
        content = {}
        content[key] = source_content
        merge(content, target_content)
    save_content(target_filename, json.dumps(target_content, ensure_ascii=False))


def generate_languages_files(languages, template_filename):
    languages_json = []
    # Localize this language
    with codecs.open(template_filename, 'r', 'utf-8') as file_descriptor:
        template = json.load(file_descriptor)

    for lang in languages:
        all_strings = {}
        merge(template, all_strings)
        language_json_filename = os.path.join("../translations", lang, 'language.json')
        with codecs.open(language_json_filename, 'r', 'utf-8') as file_descriptor:
            data = json.load(file_descriptor)
            data["language"] = lang
            languages_json.append({
                "language": lang,
                "name": data["language_name"] if "language_name" in data else data["name"]
            })
            merge(data, all_strings)
        save_content("../static/lang_" + lang + "/language.json", json.dumps(all_strings, ensure_ascii=False))
        merge_json("../static/lang_" + lang + "/language.json", "../static/lang_" + lang + "/web_common/commonLanguage.json", 'common')
    save_content('../static/languages.json', json.dumps(languages_json, ensure_ascii=False))

languages = [o for o in os.listdir('../translations') if os.path.isdir(os.path.join('../translations',o))]
merge_json('../static/language.json','../static/web_common/commonLanguage.json','common')
generate_languages_files(languages, '../static/language.json')
