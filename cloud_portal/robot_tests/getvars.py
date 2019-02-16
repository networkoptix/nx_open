import codecs
import json
import re

#default english and default customization
def get_variables(lang="en_US", cust="default"):
    #open up the customization file we want and create a dictionary called customization_json
    with codecs.open("customizations/default_customization.json", 'r', encoding='utf-8-sig') as customization_variables:
        customization_json = json.load(customization_variables)
        #open up the translation file we want and create a dictionary called translation_variables
        with codecs.open("translations/variables_language_" + lang + ".json", 'r', encoding='utf-8-sig') as translation_variables:
            translation_variables = translation_variables.read()

            #for each key in customizaion_json find the key surrounded by % and replace it with that key's value
            for x in customization_json:
                p = re.compile('%' + x + '%')
                translation_variables = p.sub(
                    customization_json[x], translation_variables)
            #load the translation_variables into python
            translation_variables = json.loads(
                translation_variables, encoding='utf-8-sig')
            #add the LANGUAGE item to the dictionary with the value of lang
            translation_variables['LANGUAGE'] = lang
            #add the customization variables
            translation_variables.update(customization_json)
            all_variables = translation_variables
            return all_variables
