#!/usr/bin/env python

import argparse
import xml.etree.ElementTree as ET

#ElementTree works kinda strange with namespaces
ms_namespace = "http://schemas.microsoft.com/developer/msbuild/2003"

ET.register_namespace('', ms_namespace)
namespaces_dict = {'ms' : ms_namespace}

def fix_qrc(root):
    #xpath = "./Project/ItemGroup/CustomBuild[contains(@Include,'.qrc')]/AdditionalInputs"

    xpath = "./ms:ItemGroup/ms:CustomBuild"
    customBuildNodes = root.findall(xpath, namespaces_dict)

    nodes = [node for node in customBuildNodes if '.qrc' in node.get('Include')]
    for node in nodes:
        print node.get('Include')

    if len(nodes) < 1:
        print "Qrc node was not found"

    for node in nodes:
        inputsNodes = node.findall("./ms:AdditionalInputs", namespaces_dict)
        for inputNode in inputsNodes:
            node.remove(inputNode)

def patch_project(project):
    print "Patching {0}...".format(project)

    tree = ET.parse(project)
    root = tree.getroot()
    fix_qrc(root)
    tree.write(project, encoding="utf-8", xml_declaration=True)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('project', type=str, help='Project to be patched.')
    args = parser.parse_args()
    patch_project(args.project)


if __name__ == '__main__':
    main()