'''
Script is required to update ArecontVision models xml
file using online database.
Must be run from the original file location.
'''

#script puts file with mismatches to your home directory
import requests
import re
import os
from os.path import expanduser

home_dir = expanduser("~")
av_url = "http://www.arecontvision.com/productselector.php"

site_list = set(map(lambda x:x[2:], re.findall('AV\d\d\d\d+',
    requests.get(av_url).content)))
    
f1_path = os.path.abspath(os.getcwd() + '/..' + '/..' +
    '/appserver2/static-resources/resources/camera_types/arecontvision.xml')
    
f1 = open(f1_path, 'r')

f2_path = os.path.abspath(os.getcwd() + '/..' + '/..' +
    '/appserver2/static-resources/02_insert_all_vendors.sql')
    
f2 = open(f2_path, 'r')

cur_list = set(map(lambda x:x[1:], re.findall('\"\d\d\d\d+', 
    f1.read()) + re.findall('\'\d\d\d\d+', f2.read())))
    
result_file = open(os.path.join(home_dir, "new_av_models.txt"), 'w')

for model in site_list:
    if model not in cur_list:
        result_file.write(model + '\n')

result_file.close()
