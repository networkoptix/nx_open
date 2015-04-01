candle -dAxSourceDir=..\..\..\..\build_environment\target\x86\bin\Release Product.wxs AxClient.wxs
light -ext WixUIExtension.dll -cultures:en-us Product.wixobj AxClient.wixobj -out ${finalName}
sign 