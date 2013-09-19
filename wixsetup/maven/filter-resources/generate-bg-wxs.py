import os

if __name__ == '__main__':
    os.system(r'heat dir ${ClientBgSourceDir} -wixvar -nologo -sfrag -suid -sreg -ag -srd -dir WebHelp -out ClientBg.wxs -cg ClientBgComponent -dr ${installer.customization}BgDir -var var.ClientBgSourceDir')

