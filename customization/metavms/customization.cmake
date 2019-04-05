set(build_mobile OFF)
set(enable_hanwha true)

# Branding
set(display.product.name "Nx MetaVMS")
set(product.name "Nx MetaVMS")
set(product.name.short "metavms")
set(product.appName "metavms")
set(company.name "Network Optix")
set(cloudName "Nx Meta Cloud")
set(uri.protocol "nx-metavms")
set(mediaserver_application_name "Network Optix MetaVMS Media Server")
set(mediaserver_service_name "Network Optix MetaVMS Media Server")
set(metaVersion "R7.3_mac")

# Default settings
set(backgroundImage
    "{\"enabled\": true, \"name\": \":/skin/background.png\", \"mode\": \"Crop\"}")
set(defaultWebPages
    "{\"Home Page\": \"http://networkoptix.com\", \"Support\": \"https://networkoptix.com/meta/\"}")

# Support section
set(companyUrl "http://networkoptix.com")
set(productUrl "https://networkoptix.com/meta/")
set(supportEmail "meta@networkoptix.com")
set(licenseEmail "meta@networkoptix.com")

set(installer.name "metavms")

# Custom Installer (if needed). If not leave "default"
set(customization.upgradeCode "{5c6f8f50-99d3-4bd5-9201-08e5c375bb13}")
set(customization.clientUpgradeCode "{0f2ed89b-8c56-4a6c-a858-59007eb004c0}")

set(customization.clientBundleUpgradeCode "{3401937b-5715-40f7-bf3d-d34b67b3b317}")
set(customization.serverBundleUpgradeCode "{b0f6cba5-66f5-4ecd-bba5-87064c7618a6}")
set(customization.fullBundleUpgradeCode "{b98a8e7a-2e21-472c-b951-e16f82a43798}")

set(deb.customization.company.name "networkoptix-metavms")

set(mac.sign.identity "Developer ID Application: Network Optix, Inc. (L6FE34GJWM)")
set(mac.bundle.identifier "com.networkoptix.metavms")
set(mac.protocol_handler_bundle.identifier "com.networkoptix.protocol_handler")

# Other customizations.
set(freeLicenseIsTrial "true")
set(freeLicenseCount "4")
set(freeLicenseKey "0000-0000-0000-0029")
