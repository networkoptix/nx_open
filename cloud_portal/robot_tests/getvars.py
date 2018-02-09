import codecs
import json

def get_variables(lang):
    with codecs.open("variables_"+lang+".json", 'r', 'utf-8') as file_descriptor:
        target_content = json.load(file_descriptor)
        return target_content