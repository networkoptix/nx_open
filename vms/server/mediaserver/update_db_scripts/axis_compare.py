'''
Script is required to update ArecontVision models xml file using online database.
Must be run from the repository root directory.
'''


#script puts file with mismatches to your home directory
import requests, re, os
from os.path import expanduser

def is_empty(any_structure):
    if any_structure:
        return False
    else:
        return True

api_key = "apikey da6cac02-e554-44c5-8125-1281982c3cdb"
headers={"authorization": api_key}
home_dir = expanduser("~")

axis_base_url = "https://www.axis.com/api/pia/v2/items"
parameters = ""
parameters += '?categories=cameras,encoders,'
parameters += 'doorstations,'
parameters += '&fields=none,category,categories,'
parameters += 'id,name,properties,parents,type&orderBy=name'
parameters += '&propertyFilterMap=%7B%22compare%22:1,%22series%22:%22'
parameters += '!Companion%22%7D&state=40&tagKey=prodsel'
parameters += '&type=ProductVariant'
axis_url = axis_base_url + parameters

get_list = requests.get(axis_url, headers = headers).json()
site_list = {}

for i in range(len(get_list)):
    model = get_list[i]['name'].encode('utf-8')
    fps = get_list[i]['properties']['MaxFPS'].encode('utf-8')
    a_series = re.findall('AXIS\s\S\d\d+', model)
    af_series = re.findall('AXIS\s\FA\d+', model)
    x_series = re.findall('X\S\d+\-\S\d+', model)
    d_series = re.findall('D\d+\S+\s\S+', model)
    if not is_empty(a_series):
        model = a_series[0][5:].lower()
    elif not is_empty(x_series):
        model = x_series[0].lower()
    elif not is_empty(af_series):
        model = af_series[0][5:].lower()
    elif not is_empty(d_series):
        model = d_series[0].lower()
    else: 
        print model
        break
    upd = {model : fps}
    site_list.update(upd)


xml_path = '/appserver2/static-resources/resources/camera_types/axis.xml'
old_db_path = '/appserver2/static-resources/02_insert_all_vendors.sql'
f1_path = os.path.abspath(os.getcwd() + '/..' + '/..' + xml_path)
f2_path = os.path.abspath(os.getcwd() + '/..' + '/..' + old_db_path)
f1 = open(f1_path, 'r')
f2 = open(f2_path, 'r')
cur_list_draft = set(re.findall('AXIS\w\d\d\d+', f1.read()) + 
    re.findall('AXIS\w\d\d\d+', f2.read()))
cur_list = set(map(lambda x:x.lower()[4:], cur_list_draft))
result_file = open(os.path.join(home_dir, "new_axis_models.txt"), 'w')

for  model, fps in site_list.iteritems():
    if model not in cur_list:
        result_file.write(('New model='+model)+('MaxFPS='+fps+'\n').rjust(30))

result_file.close()
