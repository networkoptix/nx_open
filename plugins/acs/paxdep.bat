rem @echo off

set CONFIG=%1

if [%1] == [] set CONFIG=Debug

copy C:\develop\netoptix_vms\plugins\nxinterface\x86\bin\%CONFIG%\NxWitnessInterface.1.dll "C:\Program Files (x86)\Paxton Access\Access Control"
copy C:\develop\netoptix_vms\plugins\nxcontrol\x86\bin\%CONFIG%\NxWitnessControl.1010.dll "C:\Program Files (x86)\Paxton Access\Access Control"
copy C:\develop\netoptix_vms\build_environment\target\x86\bin\%CONFIG%\Interop.NxWitness_1010.dll "C:\Program Files (x86)\Paxton Access\Access Control"
copy C:\develop\netoptix_vms\build_environment\target\x86\bin\%CONFIG%\AxInterop.NxWitness_1010.dll "C:\Program Files (x86)\Paxton Access\Access Control"
copy C:\develop\netoptix_vms\plugins\acs\paxton\x86\bin\%CONFIG%\NxWitness.OemDvrMiniDriver.Merged.dll "C:\Program Files (x86)\Paxton Access\Access Control\DvrMiniDrivers"
     