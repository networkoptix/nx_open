'''
Script is required to update ArecontVision models xml
file using online database.
Must be run from the original file location.
'''

# script puts file with mismatches to your home directory
import requests
import re
import os
from os.path import expanduser
from xml.dom import minidom
import unicodedata

home_dir = expanduser("~")
av_url = "http://www.arecontvision.com/productselector.php"

site_list_draft = set(map(lambda x: x[2:], re.findall('AV\d+\w+',
    requests.get(av_url).content)))
site_list = []
for item in site_list_draft:
    match_0 = re.match('\d\d[A-Za-z]+', item)
    match_1 = re.match('\d\d\d[A-Za-z]+', item)
    match_2 = re.match('\d\d\d\d+', item)
    for match in [match_0, match_1, match_2]:
        if match is not None:
            site_list.append(match.group(0))

site_set = set(site_list)
f1_path = os.path.abspath(os.getcwd() + '/..' + '/..' +
    '/appserver2/static-resources/resources/camera_types/arecontvision.xml')

f2_path = os.path.abspath(os.getcwd() + '/..' + '/..' +
    '/appserver2/static-resources/02_insert_all_vendors.sql')
f2 = open(f2_path, 'r')

xml_list = []
arecont_xml = minidom.parse(f1_path)
arecont_xml_models = arecont_xml.getElementsByTagName('resource')
for model in arecont_xml_models:
    match = re.match('\d+\w+', model.attributes['name'].value)
    if match is not None:
        xml_list.append(str(match.group(0)))

sql_list = re.findall('\d\d\d\d+', f2.read())

cur_set = set(xml_list + sql_list)
result_file = open(os.path.join(home_dir, "new_av_models.txt"), 'w')

for model in site_set:
    if model not in cur_set:
        result_file.write(model + '\n')

result_file.close()
