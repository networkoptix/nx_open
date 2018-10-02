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
    languages_names = []

    translations_path = os.path.join("../../../..", "translations")
    for lang in os.listdir(translations_path):

        if '.' in lang:
            continue

        language_json_filename = os.path.join("../../../..", "translations", lang, 'language.json')

        if not os.path.isfile(language_json_filename): # ignore incomplete languages without language.json
            sys.stderr.write('ERROR: For BORIS to fix: language.json is missing. '
                             'File: ' + language_json_filename + '\n')
            continue

        print("Load: " + language_json_filename)

        with codecs.open(language_json_filename, 'r', 'utf-8') as file_descriptor:
            data = json.load(file_descriptor)
            name = data["language_name"]

            # validate that there are no dublicates in languages_json structure
            if not name or name == 'LANGUAGE_NAME':
                sys.stderr.write('ERROR: For BORIS to fix: language.json has wrong or missing language_name. '
                                 'File: ' + language_json_filename + '\n')
                name = lang
            if name in languages_names:
                raise ValueError('CRITICAL ERROR: For BORIS to fix: language.json has not unique language_name. '
                                 'File: ' + language_json_filename)
            languages_names.append(name)

            languages_json.append({
                "language": lang,
                "name": name
            })

    print(languages_json)
    save_content('static/languages.json', json.dumps(languages_json, ensure_ascii=False))


generate_languages_files()
