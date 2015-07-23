import requests
import re
import os

site_list = set(map(lambda x:x[2:], re.findall('AV\d\d\d\d+', requests.get("http://www.arecontvision.com/productselector.php").content)))
f1_path = os.path.abspath(os.path.join(os.getcwd()+'/appserver2/maven/bin-resources/resources/resources/arecontvision.xml'))
f2_path = os.path.abspath(os.path.join(os.getcwd()+'/appserver/resources/arecontvision.xml'))
f1 = open(f1_path, 'r')
f2 = open(f2_path, 'r')
cur_list = set(map(lambda x:x[1:], re.findall('\"\d\d\d\d+', f1.read()) + re.findall('\"\d\d\d\d+', f2.read())))
result_file = open('new_av_models.txt', 'w')

for model in site_list:
    if model not in cur_list:
        result_file.write(model + '\n')
        
result_file.close()