# Components Namings
set(parent.customization "default")
set(product.name "Nx Witness Chinese")
set(display.product.name "Nx Witness")
set(product.name.short "hdwitness_all_cn")
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

set(installer.name "nxwitness_cn")

# Custom Installer (if needed). If not leave "default"
set(customization.upgradeCode "{15fba27b-3222-4218-8142-45a79f900462}")
set(customization.clientUpgradeCode "{d36f78de-5b91-443e-9c44-b597c8ec9911}")
set(customization.serverUpgradeCode "{43b8cd49-50a7-43f4-a669-bcbb80891e88}")
set(customization.clientBundleUpgradeCode "{147fb03d-5f8d-400f-84c1-fe03f6084b9d}")
set(customization.serverBundleUpgradeCode "{797d9453-ce53-4851-bf52-f97439c1708f}")
set(customization.fullBundleUpgradeCode "{0fa82b11-c467-4a97-b3d3-b9e811e9e432}")
set(customization.nxtoolBundleUpgradeCode "{aad72622-10ee-40e5-b52c-a7305ec0aa05}")

set(deb.customization.company.name "networkoptix")

set(mac.sign.identity "Developer ID Application: Network Optix, Inc. (L6FE34GJWM)")
set(mac.app.sign.identity "3rd Party Mac Developer Application: Network Optix, Inc. (L6FE34GJWM)")
set(mac.pkg.sign.identity "3rd Party Mac Developer Installer: Network Optix, Inc. (L6FE34GJWM)")
set(mac.bundle.identifier "com.networkoptix.NXWitnessCN")
set(mac.protocol_handler_bundle.identifier "com.networkoptix.protocol_handlerCN")

# Other customizations.
set(freeLicenseIsTrial "true")
set(freeLicenseCount "4")
set(freeLicenseKey "0000-0000-0000-0020")

# Installer Customizations
set(installer.language "zh_TW")
set(installer.cultures "zh-tw")
set(sign.password "qweasd123")
set(sign.cer "app.p12")

# Localization
set(help.language "chinese")
set(defaultTranslation zh_TW)
set(additionalTranslations zh_CN)

# this is required because of strange maven replacement (_ > -)
set(customization "default_cn")
