import os
import shutil

import pbs
from pbs import Command, which

import _winreg 

COMPANY = 'Digital Watchdog'
PRODUCT = 'DW Spectrum'

def enum_key(hKey):
    i = 0
    try:
        while True:
            yield _winreg.EnumKey(hKey, i)
            i += 1
    except WindowsError:
        hKey.Close()

def traverse(root, key):
    hKey = _winreg.OpenKey(root, key);

    for strSubKey in enum_key(hKey):
        strFullSubKey = key + "\\" + strSubKey

        for skey in traverse(root, strFullSubKey):
            yield skey

        yield strFullSubKey

    hKey.Close();

def reg_key_exists(root, key):
    try:
        h_key = _winreg.OpenKey(root, key);
        _winreg.CloseKey(h_key)
        return True
    except WindowsError:
        return False

def reg_delete_key(root, key):
    for item in list(traverse(root, key)):
        _winreg.DeleteKey(root, item);

    _winreg.DeleteKey(root, key);

def hdwitness_product_code():
    key = r"Software\Microsoft\Windows\CurrentVersion\Uninstall"
    h_key = _winreg.OpenKey(_winreg.HKEY_LOCAL_MACHINE, key)
    try:
        for subkey in enum_key(h_key):
            if not subkey.startswith('{'):
                continue

            try:
                h_subkey = _winreg.OpenKey(h_key, subkey)
                if _winreg.QueryValueEx(h_subkey, 'DisplayName')[0] == PRODUCT:
                    return subkey
            except WindowsError:
                pass
            finally:
                _winreg.CloseKey(h_subkey)
    finally:
        _winreg.CloseKey(h_key)
            

#get_product_code = Command(which('cscript.exe'))
#product_code = get_product_code('/NoLogo', 'productcode.js', 'hdwitness-2.3.0.0-windows-x64-release-full-beta.msi').strip()

product_code = hdwitness_product_code()
if product_code:
    print "Uninstalling %s" % PRODUCT
    msiexec = Command(which('msiexec.exe'))
    msiexec('/q', '/x', product_code)

optix_data_path = r"C:\Windows\System32\config\systemprofile\AppData\Local\%s" % COMPANY
if os.path.exists(optix_data_path):
    print 'Wiping files'
    shutil.rmtree(optix_data_path)

optix_reg_path = r"Software\%s" % COMPANY
if reg_key_exists(_winreg.HKEY_LOCAL_MACHINE, optix_reg_path):
    print 'Wiping registry'
    reg_delete_key(_winreg.HKEY_LOCAL_MACHINE, optix_reg_path);

#print "Installing %s" % PRODUCT
#Command('inst.bat')()