#!/usr/bin/env python

import argparse
import xml.etree.ElementTree as ET
from xml.etree.ElementTree import Element

#ElementTree works kinda strange with namespaces
ms_namespace = "http://schemas.microsoft.com/developer/msbuild/2003"

ET.register_namespace('', ms_namespace)
namespaces_dict = {'ms' : ms_namespace}

parent_map = {}
arch = 'x64'

def indent(elem, level=0):
    i = "\n" + level*"  "
    if len(elem):
        if not elem.text or not elem.text.strip():
            elem.text = i + "  "
        if not elem.tail or not elem.tail.strip():
            elem.tail = i
        for elem in elem:
            indent(elem, level+1)
        if not elem.tail or not elem.tail.strip():
            elem.tail = i
    else:
        if level and (not elem.tail or not elem.tail.strip()):
            elem.tail = i

def add_prebuild_events(root):
    """
    <Target Name="BeforeClean">
        <Message Text="Cleaning moc generated files"/>
        <Exec Command="del \$\(ProjectDir\)..\\\$\(Platform\)\\build\\${arch}\\generated\\moc_*.* /F /Q" />
    </Target>    
    <ItemDefinitionGroup>
        <Link>
            <SubSystem Condition="'%(Link.SubSystem)'=='' Or '%(Link.SubSystem)'=='NotSet'">Windows</SubSystem>
        </Link>
        <PreBuildEvent>
            <Command>\$\(ProjectDir\)../${arch}/build_moc.bat \$\(ProjectDir\)../${arch} \$\(Configuration\)</Command>
        </PreBuildEvent>
    </ItemDefinitionGroup>    
    """
    print "Adding custom header with pre-build steps"
       
    target = Element('Target', {'Name': 'BeforeClean'})
    root.insert(0, target)   
    target.append(Element('Message', {'Text': 'Cleaning moc generated files'}))
    target.append(Element('Exec', {'Command': 'del $(ProjectDir)..\\$(Platform)\\build\\{0}\\generated\\moc_*.* /F /Q'.format(arch)}))
    indent(target, 1)
    
    group = Element('ItemDefinitionGroup')
    root.insert(1, group)
    
    link = Element('Link')
    group.append(link)
    subsystem = Element('SubSystem', {'Condition': "'%(Link.SubSystem)'=='' Or '%(Link.SubSystem)'=='NotSet'"})
    subsystem.text = 'Windows'
    link.append(subsystem)
    
    pre = Element('PreBuildEvent')
    group.append(pre)
    command = Element('Command')
    pre.append(command)   
    command.text = "$(ProjectDir)../{0}/build_moc.bat $(ProjectDir)../{0} $(Configuration)".format(arch)
    
    indent(group, 1)
    

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
            files = inputNode.text.split(';')
            allowed = [f for f in files if not f.endswith('.png') and not f.endswith('.ico') and not f.endswith('.qm')]
            inputNode.text = ';'.join(allowed)

def patch_project(project):
    print "Patching {0}...".format(project)
    tree = ET.parse(project)
    root = tree.getroot()
    
    global parent_map
    parent_map = {c:p for p in tree.iter() for c in p}
    
    fix_qrc(root)
    fix_mocables(root)
    add_prebuild_events(root)
    tree.write(project, encoding="utf-8", xml_declaration=True)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('project', type=str, help='Project to be patched.')
    parser.add_argument('-a', '--arch', help='Target architecture.')
    args = parser.parse_args()
    
    if args.arch:
        global arch
        arch = args.arch
    
    patch_project(args.project)


if __name__ == '__main__':
    main()