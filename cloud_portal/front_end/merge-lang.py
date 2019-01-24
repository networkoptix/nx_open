import json


def sortdict(dct):
    kys = dct.keys()
    kys.sort()
    from collections import OrderedDict
    d = OrderedDict()
    for x in kys:
        for k, v in dct.iteritems():
            if (k == x):
                d[k] = v
    return d


def merge_two_json(x, y):
    z = x.copy()  # start with x's keys and values
    z.update(y)   # modifies z with y's keys and values & returns None

    return sortdict(z)


base_file = json.load(open('app/language_i18n.json', 'r'))
static_file = json.load(open('app/language_i18n_static.json', 'r'))

with open("./app/language_i18n.json", "wb") as outfile:
    json.dump(merge_two_json(base_file, static_file), outfile)
