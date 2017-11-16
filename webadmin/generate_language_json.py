import os
import json
import errno
import codecs
from collections import OrderedDict


def make_dir(filename):
    dir_name = os.path.dirname(filename)
    if not os.path.exists(dir_name):
        try:
            os.makedirs(dir_name)
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


def merge_json(target_filename, source1_filename, source2_filename, key=None):
    if not os.path.exists(source1_filename) or not os.path.exists(source2_filename):
        return
    with codecs.open(source1_filename, 'r', 'utf-8') as file_descriptor:
        target_content = json.load(file_descriptor, object_pairs_hook=OrderedDict)
    with codecs.open(source2_filename, 'r', 'utf-8') as file_descriptor:
        source_content = json.load(file_descriptor, object_pairs_hook=OrderedDict)

    if key is None:
        merge(source_content, target_content)
    else:
        content = {
            key: source_content
        }
        merge(content, target_content)
    save_content(target_filename, json.dumps(target_content, ensure_ascii=False, indent=4, separators=(',', ': ')))


merge_json('.tmp/language.json', 'app/language.json', 'app/web_common/commonLanguage.json', 'common')
