def fixasfiles():
    fragment1 = r"""            <Component Id="ecs.exe" Guid="*">
                <File Id="ecs.exe" KeyPath="yes" Source="$(var.AppServerSourceDir)\ecs.exe" />
            </Component>
"""

    fragment2 = """            <ComponentRef Id="ecs.exe" />
"""

    text = open('AppServerFiles.wxs', 'r').read()
    text = text.replace(fragment1, '').replace(fragment2, '')
    open('AppServerFiles.wxs', 'w').write(text)

if __name__ == '__main__':
    fixasfiles()