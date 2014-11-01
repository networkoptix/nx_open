@echo off
SET PASSWORD=${sign.password}
SET CERTIFICATE=${sign.intermediate.cer}

IF NOT [%PASSWORD%] == [] (
    IF NOT [%CERTIFICATE%] == [] (
        call ${environment.dir}\bin\signtool.exe sign /ac ${sign.intermediate.cer} /a /v /p ${sign.password} /d "${company.name} ${display.product.name}" /f ${sign.cer} bin\${finalName}.msi
    ) ELSE (
        call ${environment.dir}\bin\signtool.exe sign /a /v /p ${sign.password} /d "${company.name} ${display.product.name}" /f ${sign.cer} bin\${finalName}.msi
    )    
) ELSE call ${environment.dir}\bin\signtool.exe sign /d "${company.name} ${display.product.name}" /f ${sign.cer} bin\${finalName}.msi