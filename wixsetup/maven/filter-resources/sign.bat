@echo off
SET PASSWORD=${sign.password}
SET CERTIFICATE=${sign.intermediate.cer}

IF NOT [%PASSWORD%] == [] (
    IF NOT [%CERTIFICATE%] == [] (
        call ${environment.dir}\bin\signtool.exe sign /td sha256 /fd sha256 /tr http://tsa.startssl.com/rfc3161 /ac ${sign.intermediate.cer} /a /v /p ${sign.password} /d "${company.name} ${display.product.name}" /f ${sign.cer} %*
    ) ELSE (
        call ${environment.dir}\bin\signtool.exe sign /td sha256 /fd sha256 /tr http://tsa.startssl.com/rfc3161 /a /v /p ${sign.password} /d "${company.name} ${display.product.name}" /f ${sign.cer} %*
    )    
) ELSE call ${environment.dir}\bin\signtool.exe sign /td sha256 /fd sha256 /tr http://tsa.startssl.com/rfc3161 /d "${company.name} ${display.product.name}" /f ${sign.cer} %*