import os

def fixasfiles():
    fragment1 = r'''Id="jquery.js"'''

    fragment2 = '''Id="skawixjquery.js"'''

    fragment3 = r'''Id="index.html"''' 

    fragment4 = '''Id="skawixindex.html"'''

    text = open('ClientHelp.wxs', 'r').read()
    text = text.replace(fragment1, fragment2)
    text = text.replace(fragment3, fragment4)
    open('ClientHelp.wxs', 'w').write(text)

if __name__ == '__main__':
    os.system(r'heat dir ..\..\help\HTML -wixvar -nologo -sfrag -suid -sreg -ag -srd -dir WebHelp -out ClientHelp.wxs -cg ClientHelpComponent -dr ${installer.customization}HelpDir -var var.ClientHelpSourceDir')
    fixasfiles()

