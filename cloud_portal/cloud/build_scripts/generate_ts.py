"""
    This file extracts string constants from html files, email templates and language scripts.
    Output is a standard QT-translation file (.ts)
"""

import os
import re
import xml.etree.ElementTree as eTree
from xml.dom import minidom


def unique_list(iter_list):
    return [e for i, e in enumerate(iter_list) if iter_list.index(e) == i]


def process_js_file(file_name):
    with open(file_name, 'r') as file_descriptor:
        data = file_descriptor.read()
        strings = re.findall("(?<=').+?(?<!\\\\)(?=')", data)
        strings = [x for x in strings if x not in ignore_strings]

        if not strings:
            return None

        return {
            'filename': file_name,
            'inline': [],
            'attributes': strings
        }


def process_html_file(file_name):
    with open(file_name, 'r') as file_descriptor:
        data = file_descriptor.read()
        inline = process_inline_text(data)
        attributes = process_attributes(data)
        if not inline and not attributes:
            return None
        return {
            'filename': file_name,
            'inline': inline,
            'attributes': attributes
        }


def process_inline_text(data):
    data = re.sub(r'<.+?>', '<>', data)
    data = re.sub(r'\{\{.+?\}\}', '', data)
    data = re.sub(r'>\s*<', '', data)
    data = re.sub(r'><', '', data)
    strings = re.split('<>', data)
    return unique_list(strings)


ignore_attributes = ('id', 'name', 'class', 'style',
                     'src',
                     'type', 'role', 'for', 'target',
                     'width', 'rows', 'cols', 'maxlength',
                     'autocomplete',
                     'process', 'form',
                     'ng\-.*?', 'aria\-.*?',
                     'bgcolor', 'content', 'xmlns', 'topmargin', 'leftmargin', 'marginheight', 'marginwidth',
                     'border', 'cellspacing', 'cellpadding', 'border', 'cellspacing' 'cellpadding', 'http\-equiv')

ignore_single_attributes = ('required', 'autofocus', 'validate-field', 'novalidate', 'uib\-.*?')

ignore_regex = (r'href="#.+?"',)
ignore_strings = ('use strict',)


def process_attributes(data):
    data = re.sub(r'<\?.+?\?>', '', data)   # Remove <?xml ... ?> - just in case
    data = re.sub(r'<!\-.+?\->', '', data)    # Remove all html-comments
    data = re.sub(r'>.*?<', '><', data)     # Remove everything except tags (clear spaces and texts)

    # Important: attributes in single quotes are not supported here
    for attr in ignore_attributes:  # Here we remove all attributes which do not contain language strings.
        data = re.sub('\s+' + attr + '=".+?"', '', data)    # remove ignore_attribute="some value" with double-quotes
        data = re.sub('\s+' + attr + '=[^\s>]+', '', data)  # remove ignore_attribute=some_value (after minhtml)

    for attr in ignore_single_attributes:   # ignore attributes which can be used without values
        data = re.sub('\s+' + attr + '=".+?"', '', data)        # remove attribute if it has value in quotes anyway
        data = re.sub('\s+' + attr + '=[^\s>]+', '', data)      # remove attribute if it has value without quotes anyway
        data = re.sub('\s+' + attr + '(?=[\s/>])', '', data)     # remove attribute without value

    for regex in ignore_regex:
        data = re.sub(regex, '', data)

    data = re.sub(r'<\S+?\s*/?>', '', data)     # remove tags without attributes
    data = re.sub(r'<\S+', '', data)            # remove tags
    data = re.sub(r'/?>', '', data)             # remove closing brackets
    data = re.sub(r'\S+?="?\{\{.+?\}\}"?', '', data)    # remove template constants: {{something}}
    data = re.sub(r'"\'', '"', data)            # replace double-quotes "'something'" -> "something"
    data = re.sub(r'\'"', '"', data)            # replace double-quotes "'something'" -> "something"

    strings = re.split('"?\s*\S+?="?', data)    # split, ignoring attribute names
    return unique_list(strings)


def generate_ts(data):
    root_element = eTree.Element("TS", version="2.1", language="en_US", sourcelanguage="en")

    for record in data:
        context = eTree.SubElement(root_element, "context")
        file_name = record['filename']
        eTree.SubElement(context, "name").text = file_name
        first = True
        for string in record['inline'] + record['attributes']:
            # print(string)
            if not string.strip() or re.match('^\W+$', string):
                continue
            message = eTree.SubElement(context, "message")
            location = eTree.SubElement(message, "location")
            if first:
                location.set("filename", file_name)
                first = False
            eTree.SubElement(message, "source").text = string.strip()
            eTree.SubElement(message, "translation").text = ' '

    rough_string = eTree.tostring(root_element, 'utf-8')
    parsed = minidom.parseString(rough_string)
    return parsed.toprettyxml(indent="  ", encoding='UTF-8')


def extract_strings(file_root_dir, file_filter, dir_exclude=None, mode='html'):
    all_strings = []
    for root, subs2, files in os.walk(file_root_dir):
        if dir_exclude and root.endswith(dir_exclude):
            continue
        for filename in files:
            if filename.endswith(file_filter):
                if mode == 'js':
                    result = process_js_file(os.path.join(root, filename))
                else:
                    result = process_html_file(os.path.join(root, filename))
                if result:
                    all_strings.append(result)
    return all_strings


def format_ts(strings, file_name):
    xml_content = generate_ts(strings)
    with open(file_name, "w") as xml_file:
        xml_file.write(xml_content)


js_strings = extract_strings('../static/scripts', 'language.js', mode='js')
html_strings = extract_strings('../static/views', '.html')  # , dir_exclude='static'
format_ts(js_strings + html_strings, "cloud_portal.ts")

template_strings = extract_strings('../notifications/static/templates', '.mustache')
format_ts(template_strings, "templates.ts")
