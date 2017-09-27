# python script generates structure.json based on directory

'''
https://networkoptix.atlassian.net/browse/CLOUD-1400:

Scripts reads directory, iterates through every file and adds them to context:
If file is image - add it to a special context 'graphics', specify strictly its format, width and height
if file is text - add a separate context and try to parse it for CMS tags %...% (just like readstructure does)
otherwise - add file as file to a special context 'static'
Output must contain well-formatted json with all data + plus empty values for label, value, description for every record (for human to be able to easily fill that data in)
If that directory already has cms_structure.json file - then script should merge two of them using names as keys
'''


# 1. iterate over every directory and file in work directory
# 2. create context for each
import codecs
import json
from controllers import generate_structure

DIRECTORY = '../../../customization/default/'
STRUCTURE_FILE = 'vms_structure.json'
PRODUCT_NAME = 'vms'


def save_structure(filename, structure):
    content = json.dumps(structure, ensure_ascii=False, indent=4, separators=(',', ': '))
    with codecs.open(filename, "w", "utf-8") as file:
        file.write(content)

cms_structure = generate_structure.from_directory(DIRECTORY, PRODUCT_NAME)
save_structure(STRUCTURE_FILE, cms_structure)
