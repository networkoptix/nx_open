@echo WinRM warns and doesn't work on public networks.
@echo Client setup is not required but is here for debugging purposes.
powershell -Command "Set-NetConnectionProfile -NetworkCategory Private"
call winrm quickconfig -quiet
call winrm set winrm/config @{MaxEnvelopeSizekb="65536"}
call winrm set winrm/config/Service @{AllowUnencrypted="true"}
call winrm set winrm/config/Service/Auth @{Basic="true"}
call winrm set winrm/config/Client @{AllowUnencrypted="true"}
call winrm set winrm/config/Client @{TrustedHosts="*"}
call winrm set winrm/config/Client/Auth @{Basic="true"}
call sc config WinRM start= auto
