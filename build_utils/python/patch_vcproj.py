#!/usr/bin/env python

import argparse
import xml.etree.ElementTree as ET

#ElementTree works kinda strange with namespaces
ms_namespace = "http://schemas.microsoft.com/developer/msbuild/2003"

ET.register_namespace('', ms_namespace)
namespaces_dict = {'ms' : ms_namespace}

parent_map = {}

def fix_mocables(root):
    """Disabling moc preprocessor steps, since we do it with jom."""
    #xpath = "./Project/ItemGroup/CustomBuild[contains(@Include,'.h')]"

    xpath = "./ms:ItemGroup/ms:CustomBuild"
    customBuildNodes = root.findall(xpath, namespaces_dict)

    nodes = [node for node in customBuildNodes if node.get('Include').endswith('.h')]
    if len(nodes) < 1:
        print "Mocable nodes were not found"
        return

    print "Removing moc custom build steps"
    nodesToRemove = [parent_map[node] for node in nodes]
    for itemGroupNode in nodesToRemove:
        root.remove(itemGroupNode)        

def fix_qrc(root):
    """Removing additional inputs from qrc, since we rebuild it manually."""
    #xpath = "./Project/ItemGroup/CustomBuild[contains(@Include,'.qrc')]/AdditionalInputs"

    xpath = "./ms:ItemGroup/ms:CustomBuild"
    customBuildNodes = root.findall(xpath, namespaces_dict)

    nodes = [node for node in customBuildNodes if node.get('Include').endswith('.qrc')]
    if len(nodes) < 1:
        print "Qrc node was not found"
        return

    print "Removing AdditionalInputs from qrc custom build step"
    for node in nodes:
        inputsNodes = node.findall("./ms:AdditionalInputs", namespaces_dict)
        for inputNode in inputsNodes:
            node.remove(inputNode)

def patch_project(project):
    print "Patching {0}...".format(project)
    tree = ET.parse(project)
    root = tree.getroot()
    
    global parent_map
    parent_map = {c:p for p in tree.iter() for c in p}
    
    fix_qrc(root)
    fix_mocables(root)
    tree.write(project, encoding="utf-8", xml_declaration=True)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('project', type=str, help='Project to be patched.')
    args = parser.parse_args()
    patch_project(args.project)


if __name__ == '__main__':
    main()