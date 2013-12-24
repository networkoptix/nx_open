import os

def fixasfiles():
    text = open('ClientVox.wxs', 'r').read()
    for xfrom, xto in fragments:
        text = text.replace(xfrom, xto)
    open('ClientVox.wxs', 'w').write(text)

if __name__ == '__main__':
    os.system(r'heat dir ${ClientVoxSourceDir} -wixvar -nologo -sfrag -suid -sreg -ag -srd -dir WebHelp -out ClientVox.wxs -cg ClientVoxComponent -dr ${customization}VoxDir -var var.ClientVoxSourceDir')
    #fixasfiles()