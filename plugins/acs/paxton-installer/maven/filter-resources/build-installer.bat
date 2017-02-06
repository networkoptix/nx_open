candle -dAxSourceDir=${libdir}\bin\Release Product.wxs AxClient.wxs
light -ext WixUIExtension.dll -cultures:en-us Product.wixobj AxClient.wixobj -out ${artifact.name.paxton}.msi
signtool.exe sign /a /v /p ${sign.password} /d "${display.product.name} Paxton Plugin" /f ${sign.cer} ${artifact.name.paxton}.msi