import json


def merge_two_json(x, y):
    z = x.copy()  # start with x's keys and values
    z.update(y)   # modifies z with y's keys and values & returns None

    return z


with open('app/language_i18n.json', 'r') as auto_language:
    base_file = json.load(auto_language)

with open('app/language_i18n_static.json', 'r') as static_language:
    static_file = json.load(static_language)

with open("./app/language_i18n.json", "w") as outfile:
    json.dump(merge_two_json(base_file, static_file), outfile, indent=4, sort_keys=True, separators=(',', ': '))
