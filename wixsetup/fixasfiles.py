def fixasfiles():
    fragment1 = r"""            <Component Id="appserver.exe" Guid="*">
                <File Id="appserver.exe" KeyPath="yes" Source="$(var.AppServerSourceDir)\appserver.exe" />
            </Component>
"""

    fragment2 = """            <ComponentRef Id="appserver.exe" />
"""

    text = open('AppServerFiles.wxs', 'r').read()
    text = text.replace(fragment1, '').replace(fragment2, '')
    open('AppServerFiles.wxs', 'w').write(text)

if __name__ == '__main__':
    fixasfiles()