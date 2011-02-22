rem To run this script install WiX 3.5 from wix.sourceforge.net and add its bin dir to system path

call ..\src\version.bat
call gen_fragments.bat 

candle -dORGANIZATION_NAME="%ORGANIZATION_NAME%" -dAPPLICATION_NAME="%APPLICATION_NAME%" -dAPPLICATION_VERSION="%APPLICATION_VERSION%" -out obj\Release\ -ext WixFirewallExtension.dll -ext WixUIExtension.dll -ext WixUtilExtension.dll *.wxs
del bin\Release\UniversalClientSetup-%APPLICATION_VERSION%.msi
light -cultures:en-US -ext WixFirewallExtension.dll -ext WixUIExtension.dll -ext WixUtilExtension.dll -out bin\Release\UniversalClientSetup-%APPLICATION_VERSION%.msi -pdbout bin\Release\UniversalClientSetup.wixpdb obj\Release\*.wixobj
cscript FixExitDialog.js bin\Release\UniversalClientSetup-%APPLICATION_VERSION%.msi
