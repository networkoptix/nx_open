call install.bat
call "c:\Program Files (x86)\Network Optix\VMS\Client\client.exe" --test-timeout 5000 --test-resource-substring Server > test.txt
call uninstall.bat