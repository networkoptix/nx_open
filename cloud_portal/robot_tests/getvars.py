import codecs
import json
import re

def get_variables(lang="en_US"):
    with codecs.open("customizations/variables_customization.json", 'r') as customization_variables:
        customization_json = json.load(customization_variables)
        
        with codecs.open("translations/variables_"+lang+".json", 'r') as translation_variables:
            translation_variables = translation_variables.read()
            for x in customization_json:
                p = re.compile(x)
                target_content = p.sub(customization_json[x], translation_variables)
                target_content =  json.loads(target_content)
            return target_content
