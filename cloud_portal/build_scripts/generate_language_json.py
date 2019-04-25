import os
import json
import errno
import codecs
import sys

US_LANGUAGE_NAME = "English (US)"


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
    # Localize this language
    with codecs.open(template_filename, 'r', 'utf-8') as file_descriptor:
        template = json.load(file_descriptor)

    with codecs.open('static/language_i18n.json', 'r', 'utf-8') as file_descriptor:
        i18n_template = json.load(file_descriptor)

    for lang in languages:
        all_strings = {}
        merge(template, all_strings)
        language_json_filename = os.path.join("../../../..", "translations", lang, 'language.json')

        print("Load: " + language_json_filename)
        if os.path.exists(language_json_filename):
            with codecs.open(language_json_filename, 'r', 'utf-8') as file_descriptor:
                data = json.load(file_descriptor)
                data["language"] = lang

                if data["language_name"] == 'LANGUAGE_NAME':
                    sys.stderr.write('ERROR: For BORIS to fix: language.json has wrong language_name. '
                                     'File: ' + language_json_filename + '\n')
                    data["language_name"] = lang
                merge(data, all_strings)
        elif lang != 'en_US':
            sys.stderr.write('WARNING: ' + language_json_filename + ' don\'t exist.\n')
        else:
            all_strings['language_name'] = US_LANGUAGE_NAME
        save_content("static/lang_" + lang + "/language.json", json.dumps(all_strings, indent=4, ensure_ascii=False))

        # Process i18n files
        i18n_strings = {}
        merge(i18n_template, i18n_strings)
        i18n_json_filename = os.path.join("../../../..", "translations", lang, 'language_i18n.json')
        print("Load: " + i18n_json_filename)

        if os.path.exists(i18n_json_filename):
            with codecs.open(i18n_json_filename, 'r', 'utf-8') as file_descriptor:
                data = json.load(file_descriptor)
                data["language"] = lang

                if "language_name" in data:
                    if data["language_name"] == 'LANGUAGE_NAME':
                        data["language_name"] = lang
                else:
                    data["language_name"] = lang

                if data["language_name"] != lang:
                    sys.stderr.write('ERROR: For BORIS to fix: language_i18n.json has wrong language_name. File: ' + i18n_json_filename + '\n')
                merge(data, i18n_strings)
        elif lang != 'en_US':
            sys.stderr.write('WARNING: ' + i18n_json_filename + ' don\'t exist.\n')
        else:
            i18n_strings["language"] = lang
            i18n_strings["language_name"] = US_LANGUAGE_NAME

        save_content("static/lang_" + lang + "/language_i18n.json", json.dumps(i18n_strings, indent=4, ensure_ascii=False))

languages = sys.argv[1:]
if not languages:
    languages = ["en_US"]

generate_languages_files(languages, 'static/language.json')
