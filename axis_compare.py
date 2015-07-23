import requests
import re
import os

site_list_draft = re.findall('\S\d\d\d\d+', requests.get("http://www.axis.com/ca/en/general/ajax/firmware-list").content)
site_list =set(map(lambda x:x.lower(), site_list_draft))
f1_path = os.path.abspath(os.path.join(os.getcwd()+'/appserver2/maven/bin-resources/resources/resources/axis.xml'))
f2_path = os.path.abspath(os.path.join(os.getcwd()+'/appserver2/maven/bin-resources/resources/02_insert_all_vendors.sql'))
f1 = open(f1_path, 'r')
f2 = open(f2_path, 'r')
cur_list_draft = set(re.findall('AXIS\w\d\d\d+', f1.read()) + re.findall('AXIS\w\d\d\d+', f2.read()))
cur_list = set(map(lambda x:x.lower()[4:], cur_list_draft))
result_file = open('new_axis_models.txt', 'w')

for  model in site_list:
    if model not in cur_list:
        result_file.write(model+'\n')

result_file.close()