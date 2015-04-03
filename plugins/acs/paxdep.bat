@echo off

set CONFIG=%1

if [%1] == [] set CONFIG=Debug

copy C:\develop\netoptix_vms\plugins\nxinterface\x86\bin\%CONFIG%\NxInterface.1.dll "C:\Program Files (x86)\Paxton Access\Access Control"
copy C:\develop\netoptix_vms\plugins\nxcontrol\x86\bin\%CONFIG%\NxControl.1010.dll "C:\Program Files (x86)\Paxton Access\Access Control"
copy C:\develop\netoptix_vms\plugins\acs\paxton\x86\bin\%CONFIG%\NetworkOptix.OemDvrMiniDriver.Merged.dll "C:\Program Files (x86)\Paxton Access\Access Control\DvrMiniDrivers"
     