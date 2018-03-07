import codecs
import json
import re

def get_variables(lang="en_US"):
    with codecs.open("customizations/variables_customization.json", 'r', encoding='utf-8-sig') as customization_variables:
        customization_json = json.load(customization_variables)
        
        with codecs.open("translations/variables_language_"+lang+".json", 'r', encoding='utf-8-sig') as translation_variables:
            translation_variables = translation_variables.read()
            for x in customization_json:
                p = re.compile(x)
                target_content = p.sub(customization_json[x], translation_variables)

            target_content =  json.loads(target_content, encoding='utf-8-sig')
            target_content['LANGUAGE']=lang
            return target_content