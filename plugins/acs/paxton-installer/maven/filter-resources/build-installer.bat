candle -dAxSourceDir=${libdir}\bin\Release Product.wxs AxClient.wxs
light -ext WixUIExtension.dll -cultures:en-us Product.wixobj AxClient.wixobj -out ${finalName}
signtool.exe sign /a /v /p ${sign.password} /d "${display.product.name} Paxton Plugin" /f ${sign.cer} ${finalName}