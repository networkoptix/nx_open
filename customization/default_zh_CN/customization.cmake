# Components Namings
set(parent.customization "default")
set(product.name "Nx Witness Simplified Chinese")
set(display.product.name "Nx Witness")
set(product.name.short "hdwitness_cn")
set(company.name "Network Optix")

set(product.appName "hdwitness")
set(display.mobile.name "Nx Mobile")
set(short.company.name "Nx")
set(uri.protocol "nx-vms")
set(liteDeviceName "nx1")

set(backgroundImage
    "{\"enabled\": true, \"name\": \":/skin/background.png\", \"mode\": \"Crop\"}")
set(defaultWebPages
    "{\"Home Page\": \"http://networkoptix.com\", \"Support\": \"http://support.networkoptix.com\"}")

# Cloud parameters
set(cloudName "Nx Cloud")

# Support section
set(companyUrl "http://networkoptix.com")
set(productUrl "http://networkoptix.com/nxwitness-overview")
set(supportUrl "http://support.networkoptix.com")
set(supportEmail "support@networkoptix.com")
set(supportPhone "")
set(licenseEmail "support@networkoptix.com")
# end of Support section

set(installer.name "nxwitness_zh_CN")

# Custom Installer (if needed). If not leave "default"

set(customization.upgradeCode "{d3062cfe-e403-4f51-aa44-89b7e51b1452}")
set(customization.clientUpgradeCode "{f095ec38-62af-427c-ad3e-f928a5953718}")
set(customization.serverUpgradeCode "{77de59d1-9d1b-4a4a-a0ae-7f2032500dd6}")
set(customization.clientBundleUpgradeCode "{732a6919-720b-4380-ac9b-d02ede5ac2e2}")
set(customization.serverBundleUpgradeCode "{1d68ad5f-2c19-449a-9c71-c9d2457b3856}")
set(customization.fullBundleUpgradeCode "{414c9665-cd4d-4a41-a1d0-ff98d36f4664}")
set(customization.nxtoolBundleUpgradeCode "{44053de6-b65a-448d-9936-8aefddb7c85a}")

set(deb.customization.company.name "networkoptix")

set(mac.sign.identity "Developer ID Application: Network Optix, Inc. (L6FE34GJWM)")
set(mac.app.sign.identity "3rd Party Mac Developer Application: Network Optix, Inc. (L6FE34GJWM)")
set(mac.pkg.sign.identity "3rd Party Mac Developer Installer: Network Optix, Inc. (L6FE34GJWM)")
set(mac.bundle.identifier "com.networkoptix.NXWitnessCN2")
set(mac.protocol_handler_bundle.identifier "com.networkoptix.protocol_handlerCN2")

# Other customizations.
set(freeLicenseIsTrial "true")
set(freeLicenseCount "4")
set(freeLicenseKey "0000-0000-0000-0007")

# Installer Customizations
set(installer.language "zh_CN")
set(installer.cultures "zh-cn")
set(sign.password "qweasd123")
set(sign.cer "app.p12")
set(sign.intermediate.cer "")

# Localization
set(help.language "chinese")
set(defaultTranslation zh_CN)

# this is required because of strange maven replacement (_ > -)
set(customization "default_zh_CN")
