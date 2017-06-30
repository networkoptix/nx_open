@echo off
SET PASSWORD=${sign.password}
SET CERTIFICATE=${sign.intermediate.cer}
SET SIGNCER=${static.customization.dir}/wixsetup/${sign.cer}
SET TIMESTAMP=
IF NOT ["${beta}"] == ["true"] (
    SET TIMESTAMP=/tr http://tsa.startssl.com/rfc3161
) ELSE (
    SET TIMESTAMP=/tr http://timestamp.digicert.com
)

IF NOT ["${windows.skip.sign}"] == ["true"] (
    IF ["${beta}"] == ["false"] IF ["${installer.token}"] == ["true"] (
        call ${environment.dir}\bin\signtool.exe sign /td sha256 /fd sha256 %TIMESTAMP% /d "${company.name} ${display.product.name}" %*
        exit 0
    )
    IF NOT [%PASSWORD%] == [] (
        IF NOT [%CERTIFICATE%] == [] (
            set command="${environment.dir}\bin\signtool.exe" sign /td sha256 /fd sha256 %TIMESTAMP% /ac ${sign.intermediate.cer} /a /v /p ${sign.password} /d "${company.name} ${display.product.name}" /f %SIGNCER% %*
        ) ELSE (
            set command="${environment.dir}\bin\signtool.exe" sign /td sha256 /fd sha256 %TIMESTAMP% /a /v /p ${sign.password} /d "${company.name} ${display.product.name}" /f %SIGNCER% %*
        )
    ) ELSE set command="${environment.dir}\bin\signtool.exe" sign /td sha256 /fd sha256 %TIMESTAMP% /d "${company.name} ${display.product.name}" /f %SIGNCER% %*
)
echo signing %*
echo %command%
call %command%