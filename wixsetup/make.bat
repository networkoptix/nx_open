rem To run this script install WiX 3.5 from wix.sourceforge.net and add its bin dir to system path

call gen_fragments.bat 

Candle.exe -out obj\Release\ -ext WixFirewallExtension.dll -ext WixUIExtension.dll -ext WixUtilExtension.dll *.wxs
Light.exe -cultures:null -ext WixFirewallExtension.dll -ext WixUIExtension.dll -ext WixUtilExtension.dll -out bin\Release\UniversalClientSetup.msi -pdbout bin\Release\UniversalClientSetup.wixpdb obj\Release\*.wixobj