import os, sys

sys.path.append('..')

from string import Template, join
from filetypes import filetypes

header_string = """<?xml version="1.0" encoding="UTF-8"?>
<Wix xmlns="http://schemas.microsoft.com/wix/2006/wi">
<Fragment>
"""

footer_string = """
</Fragment>
</Wix>
"""

component_template_string = """     <Component Id="Eve${capitalized_ext}FileAssociation" Guid="${guid}" DiskId="1">
              <RegistryValue Root="HKCR" Type="string" Key="EVE.${ext}" Value="EVE media file (.${ext})" KeyPath="yes" />
              <RegistryValue Root="HKCR" Type="string" Key="EVE.${ext}\shell\open\command" Value="&quot;[#UniclientEXE]&quot; &quot;%1&quot;"/>
              <RegistryValue Root="HKCR" Type="string" Key="EVE.${ext}\DefaultIcon" Value="&quot;[#UniclientEXE]&quot;" />
              <RegistryValue Root="HKCR" Type="string" Key=".${ext}" Value="EVE.${ext}" />
              
              <RegistryValue Root="HKLM" Key="SOFTWARE\Classes\SystemFileAssociations\.${ext}\shell\play.EvePlayer.exe" Value="Play with $$(env.APPLICATION_NAME)" Type="string" />
              <RegistryValue Root="HKLM" Key="SOFTWARE\Classes\SystemFileAssociations\.${ext}\shell\play.EvePlayer.exe\command" Value="&quot;[#UniclientEXE]&quot; &quot;%1&quot;" Type="string" />

              <RegistryValue Root="HKCR" Type="string" Key="Applications\EvePlayer-Beta.exe\SupportedTypes" Name=".${ext}" Value=""/>
              <RegistryValue Root="HKLM" Type="string" Key="Software\Clients\Media\EVE media player\Capabilities\FileAssociations" Name=".${ext}" Value="EVE.${ext}"/>
            </Component>
"""

feature_template_string = """
      <Feature Id="Eve${capitalized_ext}FileAssociation"
               Level="1"
               AllowAdvertise="no"
               InstallDefault="local"
               Title="${upper_ext} Files (.${ext})"
               Description="Associates .${ext} files with $$(env.APPLICATION_NAME)">
        <ComponentRef Id="Eve${capitalized_ext}FileAssociation"/>
      </Feature>
"""

def gen_assoc():
    xout = open('Associations.wxs', 'w')
    print >> xout, header_string

    component_template = Template(component_template_string)
    print >> xout, '  <DirectoryRef Id="INSTALLDIR">'
    for ext, guid in filetypes:
        print >> xout, component_template.substitute(ext=ext, guid=guid, capitalized_ext=ext.capitalize())
    print >> xout, '  </DirectoryRef>'

    feature_template = Template(feature_template_string)

    print >> xout, """
        <Feature Id='EveMediaPlayerFileAssociations' Level='1' AllowAdvertise='no' InstallDefault='local' Title='File Associations'
                     Description='Registers file associations with $(env.APPLICATION_NAME).'>
              <ComponentRef Id='DummyFileAssocationFeatureComponent'/>
"""
    for ext, guid in filetypes:
        print >> xout, feature_template.substitute(ext=ext, upper_ext=ext.upper(), capitalized_ext=ext.capitalize())
    print >> xout, '  </Feature>'

    print >> xout, footer_string

    xout.close()

def gen_file_exts():
    xout = open('FileExtensions.wxs', 'w')
    print >> xout, header_string

    print >> xout, '<Property Id="FILETYPES" Admin="yes" Value="%s" />' % join(['.' + x[0] for x in filetypes],'|')

    print >> xout, footer_string

    xout.close()

if __name__ == '__main__':
    gen_assoc()
    gen_file_exts()
