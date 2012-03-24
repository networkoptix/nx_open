import _winreg

def regquerysubkeys(handle, key, keylist = []):    
    reghandle = _winreg.OpenKey(handle, key, 0, _winreg.KEY_ALL_ACCESS | _winreg.KEY_WOW64_64KEY)    

    nkeys = []
    i = 0
    try:
        while True:
            name, value, type = _winreg.EnumValue(reghandle, i)
            if type == _winreg.REG_SZ and 'optix' in value.lower():
                print key, name
#                i += 1
                _winreg.DeleteValue(reghandle, name)
            else:
                i += 1
    except _winreg.error as ex:
        if ex[0] == 259:
            pass
            #print key
        else:
            raise

    i = 0
    try:
        while True:
            subkey = _winreg.EnumKey(reghandle, i)
            i += 1
            regquerysubkeys(handle, key + subkey + "\\", keylist)
    except _winreg.error as ex:
            #If no more subkeys can be found, we can append ourself
           
            if ex[0] == 259:
                pass
                #print key
            else:
                raise
    finally:
        #do some cleanup and close the handle
        _winreg.CloseKey(reghandle)

regquerysubkeys(_winreg.HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion\\Installer\\")