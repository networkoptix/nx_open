from string import Template, join

filetypes = (
    ('avi',  '753E66D9-8015-4b4d-97CA-1E777E96E34F'),
    ('mp4',  '46397F4F-9E29-49a0-AB81-FEDF814661E9'),
    ('mkv',  '502013E6-530F-4964-A6EE-B45ABEFB78A5'),
    ('wmv',  '183AB5BA-1409-4fe2-8E43-3759BE300509'),
    ('mov',  'B9F8B8FC-4F40-4870-BC67-5B262AD9AC6A'),
    ('vob',  'AC88723D-A68C-4617-A0F8-92AF90FDF9B5'),
    ('m4v',  'D0DF03A2-038F-423e-BFB1-C935DA1CCFD3'),
    ('3gp',  '37B6723E-8B14-40a7-8D0F-B830D34F8A95'),
    ('mpeg', '4A06B0D6-7A47-494a-B829-82CCC7B0B134'),
    ('mpg',  '75CAC87E-2C68-4f0d-A737-250F3BA292B7'),
    ('mpe',  '9732F38C-B2FD-4a2e-8D3B-7BABBCC2C129'),
    ('m2ts', '71E87F51-1410-4f8f-9CA5-B230E21D8116'),
    ('flv',  '90A28113-3849-4164-8FA3-D57CF0D8C03F'),
    ('jpg',  '950A5F13-D30F-41d5-8A4D-1A4AEEE7F203'),
    ('jpeg', '155E336F-25E4-48d9-AAA8-F1176D00EB12')
)

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
