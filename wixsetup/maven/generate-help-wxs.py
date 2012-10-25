import os

def fixasfiles():
    fragments = (
        (r'''Id="jquery.js"''', '''Id="skawixjquery.js"'''),
        (r'''Id="index.html"''', '''Id="skawixindex.html"'''),
		(r'''Id="calendar_checked.png"''', '''Id="skawixcalendar_checked.png"'''),
		(r'''Id="live_checked.png"''', '''Id="skawixlive_checked.png"'''),
	)

    text = open('ClientHelp.wxs', 'r').read()
    for xfrom, xto in fragments:
        text = text.replace(xfrom, xto)
    open('ClientHelp.wxs', 'w').write(text)

if __name__ == '__main__':
    os.system(r'heat dir ..\..\help\HTML -wixvar -nologo -sfrag -suid -sreg -ag -srd -dir WebHelp -out ClientHelp.wxs -cg ClientHelpComponent -dr ${installer.customization}HelpDir -var var.ClientHelpSourceDir')
    fixasfiles()

