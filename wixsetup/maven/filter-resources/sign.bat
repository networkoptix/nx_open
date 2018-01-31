@echo off
SET PASSWORD=${sign.password}

IF NOT [%PASSWORD%] == [] (
    call ${environment.dir}\bin\signtool.exe sign /td sha256 /fd sha256 /tr http://tsa.startssl.com/rfc3161 /a /v /p ${sign.password} /d "${company.name} ${display.product.name}" /f ${certificates_path}/wixsetup/${sign.cer} %*
) ELSE call ${environment.dir}\bin\signtool.exe sign /td sha256 /fd sha256 /tr http://tsa.startssl.com/rfc3161 /d "${company.name} ${display.product.name}" /f ${certificates_path}/wixsetup/${sign.cer} %*