rem To run this script install WiX 3.5 from wix.sourceforge.net and add its bin dir to system path

Candle.exe -out obj\Debug\ -ext WixUIExtension.dll -ext WixUtilExtension.dll *.wxs
Light.exe -cultures:null -ext WixUIExtension.dll -ext WixUtilExtension.dll -out bin\Debug\UniversalClientSetup.msi -pdbout bin\Debug\UniversalClientSetup.wixpdb obj\Debug\*.wixobj