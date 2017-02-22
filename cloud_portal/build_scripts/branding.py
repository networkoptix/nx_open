import os
import re
import xml.etree.ElementTree as eTree
from os.path import join
import htmlmin

import json
import errno
import shutil
import codecs

# 1. read ts into xml
# Read branding info

branding_messages = {}


def read_branding():
    branding_file = 'branding.ts'
    tree = eTree.parse(branding_file)
    root = tree.getroot()
    for context in root.iter('context'):
        for message in context.iter('message'):
            source = message.find('source').text
            translation = message.find('translation').text
            branding_messages[source] = translation


def process_branding(content):
    for src, trans in branding_messages.iteritems():
        content = content.replace(src, trans)
    return content


def make_dir(filename):
    dirname = os.path.dirname(filename)
    if not os.path.exists(dirname):
        try:
            os.makedirs(dirname)
        except OSError as exc:  # Guard against race condition
            if exc.errno != errno.EEXIST:
                raise


def save_content(filename, content):
    if filename:
        # proceed with branding
        active_content = process_branding(content)

        if filename.endswith('.html'):
            content = htmlmin.minify(content, remove_comments=True, remove_empty_space=True,
                                     remove_optional_attribute_quotes=False)

        make_dir(filename)
        with codecs.open(filename, "w", "utf-8") as file:
            file.write(active_content)
            file.close()


def process_files(lang, root_directory, xml_filename):
    # 1. Copy all sources to target dir

    xml_filename = os.path.join("../../..", "translations", lang, xml_filename)
    tree = eTree.parse(xml_filename)

    root = tree.getroot()

    # 2. go file by file
    active_filename = ''
    target_filename = ''
    active_content = ''
    json_mode = False
    for context in root.iter('context'):
        for message in context.iter('message'):
            location = message.find('location')
            if location is not None and location.get('filename'):
                filename = location.get('filename')
                save_content(target_filename, active_content)
                active_filename = os.path.join(root_directory, filename)
                target_filename = os.path.join(root_directory, "lang_" + lang, filename)
                html_mode = active_filename.endswith('.html') or active_filename.endswith('.mustache')
                try:
                    with open(active_filename, 'r') as file_descriptor:
                        active_content = file_descriptor.read()
                except IOError as a:
                    print(a)
                    continue
                active_content = active_content.replace("LANGUAGE", lang)  # Insert language information
                if not html_mode:
                    active_content = json.dumps(json.loads(active_content), ensure_ascii=False)  # normalize json

            source = message.find('source').text

            translation = message.find('translation').text

            if translation and translation.strip():
                if html_mode:
                    # 3. replace string in file
                    if ' ' not in source:
                        # print("! replacing single word:", source, active_filename)
                        active_content = re.sub('(?<=\W)' + re.escape(source) + '(?=\W)', translation, active_content)
                    else:
                        # print("replacing:", active_filename, source, translation)
                        active_content = active_content.replace(source, translation)
                else:
                    # 4. Replacing in json mode - values only
                    active_content = re.sub('(?<=:\s")' + re.escape(source) + '(?=")', translation, active_content)

    save_content(target_filename, active_content)


def generate_languages_files(languages):
    languages_json = []
    # Localize this language
    for lang in languages:
        # copy static/views to target dir
        lang_dir = os.path.join('static', 'lang_' + lang)
        if os.path.isdir(lang_dir):
            shutil.rmtree(lang_dir)
        make_dir(lang_dir)
        shutil.copytree(os.path.join('static', 'views'), os.path.join(lang_dir, 'views'))

        process_files(lang, 'static', 'cloud_portal.ts')

        language_json_filename = os.path.join("../../..", "translations", lang, 'language.json')

        with open(language_json_filename, 'r') as file_descriptor:
            data = json.load(file_descriptor)
            data["language"] = lang
            languages_json.append(data)

    save_content('static/languages.json', json.dumps(languages_json, ensure_ascii=False))


def brand_file(filename):
    with codecs.open(filename, 'r', 'utf-8') as file_descriptor:
        active_content = file_descriptor.read()
    active_content = process_branding(active_content)
    save_content(filename, active_content)


def brand_directory(directory, file_filter, exclude_file = None):
    all_strings = []
    for root, dirs, files in os.walk(directory):
        for filename in files:
            if filename.endswith(file_filter):
                if exclude_file and filename.endswith(exclude_file):
                    continue
                brand_file(os.path.join(root, filename))


def precess_branding_for_all():
    # files to brand:

    brand_file('static/apple-app-site-association')
    brand_directory('static', '.html', 'index.html')
    brand_directory('static', '.json')
    brand_directory('templates', '.mustache')
    brand_directory('templates', '.json')
    pass


read_branding()
precess_branding_for_all()
