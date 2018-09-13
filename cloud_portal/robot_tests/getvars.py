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
                translation_variables = p.sub(customization_json[x], translation_variables)

            translation_variables =  json.loads(translation_variables, encoding='utf-8-sig')
            translation_variables['LANGUAGE']=lang
            translation_variables['PRIVACY_POLICY_URL']=customization_json['PRIVACY_POLICY_URL']
            translation_variables['SUPPORT_URL']=customization_json['SUPPORT_URL']
            translation_variables['WEBSITE_URL']=customization_json['WEBSITE_URL']
            translation_variables['THEME COLOR']=customization_json['THEME COLOR']
            translation_variables['PRODUCT NAME']=customization_json['%PRODUCT_NAME%']
            return translation_variables
