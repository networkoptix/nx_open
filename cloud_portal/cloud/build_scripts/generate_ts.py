import os
import re
import xml.etree.ElementTree as eTree
from xml.dom import minidom


def process_js_file(file_name):
    with open(file_name, 'r') as file_descriptor:
        data = file_descriptor.read()
        strings = re.findall("(?<=').+?(?<!\\\\)(?=')", data)
        if len(strings) == 0:
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
        if len(inline) == 0 and len(attributes) == 0:
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
    return list(set(strings))

ignore_attributes = ('id', 'name', 'class', 'style',
                     'src', 'href',
                     'type', 'role', 'for', 'target',
                     'width', 'rows', 'cols', 'maxlength',
                     'autocomplete',
                     'process', 'form',
                     'ng-.*?', 'aria-.*?',
                     'bgcolor', 'content', 'xmlns', 'topmargin', 'leftmargin', 'marginheight', 'marginwidth')

ignore_single_attributes = ('required', 'autofocus', 'validate-field', 'novalidate', 'href')


def process_attributes(data):
    data = re.sub(r'<!-.+?->', '><', data)
    data = re.sub(r'>.+?<', '><', data)

    for attr in ignore_attributes:
        data = re.sub('\s+' + attr + '=".+?"', '', data)
        data = re.sub('\s+' + attr + '=[^\s>]+', '', data)

    for attr in ignore_single_attributes:
        data = re.sub('\s+' + attr + '=".+?"', '', data)
        data = re.sub('\s+' + attr + '=[^\s>]+', '', data)
        data = re.sub('\s+' + attr, '', data)

    data = re.sub(r'<\S+?\s*>', '', data)
    data = re.sub(r'<\S+', '', data)
    data = re.sub(r'>', '', data)
    data = re.sub(r'\S+?="?\{\{.+?\}\}"?', '', data)
    data = re.sub(r'"\'', '"', data)
    data = re.sub(r'\'"', '"', data)
    strings = re.split('"?\s*\S+?="?', data)
    return list(set(strings))


def generate_ts(data):
    root_element = eTree.Element("TS", version="2.1", language="en_US", sourcelanguage="en")

    for record in data:
        context = eTree.SubElement(root_element, "context")
        file_name = record['filename']
        eTree.SubElement(context, "name").text = file_name
        first = True
        for string in record['inline'] + record['attributes']:
            # print(string)
            if not string.strip():
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
html_strings = extract_strings('../static/views', '.html', dir_exclude='static')
format_ts(js_strings + html_strings, "cloud_portal.ts")

template_strings = extract_strings('../notifications/static/templates', '.mustache')
format_ts(template_strings, "templates.ts")
