@echo off
SET PASSWORD=${sign.password}
SET SIGNCER=${certificates_dir}/wixsetup/${sign.cer}
SET TIMESTAMP=
IF NOT ["${beta}"] == ["true"] (
    SET TIMESTAMP=/tr http://tsa.startssl.com/rfc3161
)

IF NOT ["${windows.skip.sign}"] == ["true"] (
    IF NOT [%PASSWORD%] == [] (
        set command="${environment.dir}\bin\signtool.exe" sign /td sha256 /fd sha256 %TIMESTAMP% /a /v /p ${sign.password} /d "${company.name} ${display.product.name}" /f %SIGNCER% %*
    ) ELSE set command="${environment.dir}\bin\signtool.exe" sign /td sha256 /fd sha256 %TIMESTAMP% /d "${company.name} ${display.product.name}" /f %SIGNCER% %*
)
echo signing %*
echo %command%
call %command%
