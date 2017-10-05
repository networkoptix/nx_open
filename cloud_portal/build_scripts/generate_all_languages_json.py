import os
import json
import errno
import codecs
import sys


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


def generate_languages_files():
    languages_json = []

    translations_path = os.path.join("../../../..", "translations")
    for lang in os.listdir(translations_path):
        language_json_filename = os.path.join("../../../..", "translations", lang, 'language.json')

        print("Load: " + language_json_filename)
        with codecs.open(language_json_filename, 'r', 'utf-8') as file_descriptor:
            data = json.load(file_descriptor)
            name = data["language_name"] if "language_name" in data else data["name"]
            if name == 'LANGUAGE_NAME':
                name = lang
            languages_json.append({
                "language": lang,
                "name": name
            })
    print(languages_json)
    save_content('static/languages.json', json.dumps(languages_json, ensure_ascii=False))


generate_languages_files()
