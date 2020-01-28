import json


def filter_extracted_static(json_elem):
    keys = ["dialogs.merge", "WHEN_MERGE_FINISHES", "FAILED_SYSTEM_DESCR", "NO_SYSTEMS_",
            "registration.agreement", "merge.warning"]
    return any((key in json_elem for key in keys))


def replace_empty(json_elem):
    if isinstance(json_elem, list):
        return [replace_empty(elem) for elem in json_elem]
    elif isinstance(json_elem, dict):
        return {key: replace_empty(value) for key, value in json_elem.items()}
    else:
        return json_elem

with open('app/language_i18n.json', 'r') as language:
    json_data = json.load(language)

copy = json_data.copy()

for item in copy.keys():
    if filter_extracted_static(item):
        del json_data[item]
        continue

    json_data[item] = replace_empty(item)


with open("./app/language_i18n.json", "w") as outfile:
    json.dump(json_data, outfile, indent=4, sort_keys=True, separators=(',', ': '))
