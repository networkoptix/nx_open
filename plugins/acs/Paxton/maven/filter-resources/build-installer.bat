candle -dAxSourceDir=..\build_environment\target\x86\bin\Release Product.wxs AxClient.wxs
light Product.wixobj AxClient.wixobj -ext WixUIExtension.dll -ext WixUtilExtension.dll -out nx-paxton-plugin.1.msi