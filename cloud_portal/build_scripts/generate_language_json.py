import os
import json
import errno
import codecs
import sys

US_LANGUAGE_NAME = "English (US)"
US_LANGUAGE_CODE = "en_US"


def make_dir(filename):
    dirname = os.path.dirname(filename)
    print(f"make dir {dirname} for {filename}")
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


def merge_files(base_lang, lang, file_name):
    merged_lang = base_lang.copy()
    lang_json_file = os.path.join("../../../..", "translations", lang, file_name)
    print(f"Load {lang_json_file}")

    if os.path.exists(lang_json_file):
        with codecs.open(lang_json_file, "r", "utf-8") as file:
            translated_json = json.load(file)
            translated_json["language"] = lang

            language_name = translated_json.get("language_name", "")
            if not language_name or language_name == "LANGUAGE_NAME":
                translated_json["language_name"] = lang

            if translated_json["language_name"] == "LANGUAGE_NAME":
                sys.stderr.write(f"ERROR: For BORIS to fix: language.json has wrong language_name. "
                                 f"File: {lang_json_file}\n")
                translated_json["language_name"] = lang
            merged_lang.update(translated_json)
    elif lang != US_LANGUAGE_CODE:
        sys.stderr.write(f"WARNING: {lang_json_file} don't exist.\n")
    else:
        merged_lang["language"] = US_LANGUAGE_CODE
        merged_lang["language_name"] = US_LANGUAGE_NAME

    save_content(f"static/lang_{lang}/{file_name}",
                 json.dumps(merged_lang, indent=4, ensure_ascii=False, sort_keys=True))


def generate_languages_files(languages, template_filename):
    # Localize this language
    with codecs.open(template_filename, "r", "utf-8") as file_descriptor:
        base_lang = json.load(file_descriptor)

    with codecs.open("static/language_i18n.json", "r", "utf-8") as file_descriptor:
        base_i18n = json.load(file_descriptor)

    for lang in languages:
        merge_files(base_lang, lang, "language.json")
        merge_files(base_i18n, lang, "language_i18n.json")


languages = sys.argv[1:]
if not languages:
    languages = ["en_US"]

generate_languages_files(languages, "static/language.json")
