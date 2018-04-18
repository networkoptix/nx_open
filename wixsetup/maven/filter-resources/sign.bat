@echo off
SET PASSWORD=${sign.password}
SET CERTIFICATE=${sign.intermediate.cer}
SET TIMESTAMP=
IF NOT ["${beta}"] == ["true"] (
    SET TIMESTAMP=/tr http://timestamp.digicert.com/
)

IF NOT [%PASSWORD%] == [] (
    IF NOT [%CERTIFICATE%] == [] (
        call ${environment.dir}\bin\signtool.exe sign /td sha256 /fd sha256 %TIMESTAMP% /ac ${sign.intermediate.cer} /a /v /p ${sign.password} /d "${company.name} ${display.product.name}" /f ${sign.cer} %*
    ) ELSE (
        call ${environment.dir}\bin\signtool.exe sign /td sha256 /fd sha256 %TIMESTAMP% /a /v /p ${sign.password} /d "${company.name} ${display.product.name}" /f ${sign.cer} %*
    )
) ELSE call ${environment.dir}\bin\signtool.exe sign /td sha256 /fd sha256 %TIMESTAMP% /d "${company.name} ${display.product.name}" /f ${sign.cer} %*