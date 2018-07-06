#script puts file with mismatches to your home directory
import requests, re, os
from os.path import expanduser

def is_empty(any_structure):
    if any_structure:
        return False
    else:
        return True

referer_url = "https://www.axis.com/za/en/products/product-selector"
api_key = "apikey da6cac02-e554-44c5-8125-1281982c3cdb"
headers={"Host":"www.axis.com", "referer": referer_url, "Authorization": api_key}
home_dir = expanduser("~")
axis_url = "https://www.axis.com/api/pia/v2/items/?categories=cameras,encoders,modular&fields=properties,parents&orderBy=name&propertyFilterMap=%7B%22archived%22:0,%22compare%22:1,%22targetGrouping%22:%22--%22%7D&type=ProductVariant"
get_list = requests.get(axis_url, headers = headers).json()
site_list = {}
for i in range(len(get_list)):
    model = get_list[i]['name'].encode('utf-8')
    fps = get_list[i]['properties']['MaxFPS'].encode('utf-8')
    a_series = re.findall('AXIS\s\S\d\d+', model)
    af_series = re.findall('AXIS\s\S\S\d+', model)
    x_series = re.findall('X\S\d+\-\S\d+', model)
    if (is_empty(a_series) and is_empty(af_series)):
        model = x_series[0].lower()
    elif (is_empty(a_series) and is_empty(x_series)):
        model = af_series[0][5:].lower()
    else:
        model = a_series[0][5:].lower()
    upd = {model : fps}    
    site_list.update(upd)


xml_path = '/appserver2/static-resources/resources/camera_types/axis.xml'
old_db_path = '/appserver2/static-resources/02_insert_all_vendors.sql'
f1_path = os.path.abspath(os.path.join(os.getcwd() + xml_path))
f2_path = os.path.abspath(os.path.join(os.getcwd() + old_db_path))
f1 = open(f1_path, 'r')
f2 = open(f2_path, 'r')
cur_list_draft = set(re.findall('AXIS\w\d\d\d+', f1.read()) + re.findall('AXIS\w\d\d\d+', f2.read()))
cur_list = set(map(lambda x:x.lower()[4:], cur_list_draft))
result_file = open(os.path.join(home_dir, "new_axis_models.txt"), 'w')

for  model, fps in site_list.iteritems():
    if model not in cur_list:
        result_file.write(('New model='+model)+('MaxFPS='+fps+'\n').rjust(30))

result_file.close()