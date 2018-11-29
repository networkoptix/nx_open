set(build_nxtool ON)
set(build_paxton ON)
set(enable_hanwha true)

set(defaultSkin "dark_blue")
set(compatibleCustomizations "default_cn" "default_zh_CN")

# Components Namings
set(display.product.name "Nx Witness")
set(display_mobile_name "Nx Mobile")
set(mobile_bundle_name "NxMobile")
set(product.name "HD Witness")
set(product.name.short "hdwitness")
set(product.appName "hdwitness")
set(company.name "Network Optix")
set(liteDeviceName "nx1")
set(cloudName "Nx Cloud")
set(uri.protocol "nx-vms")

# Default settings
set(backgroundImage
    "{\"enabled\": true, \"name\": \":/skin/background.png\", \"mode\": \"Crop\"}")
set(defaultWebPages
    "{\"Home Page\": \"http://networkoptix.com\", \"Support\": \"http://support.networkoptix.com\"}")

# Support section
set(companyUrl "http://networkoptix.com")
set(productUrl "http://networkoptix.com/nxwitness-overview")
set(supportUrl "http://support.networkoptix.com")
set(licenseEmail "https://support.networkoptix.com/hc/en-us/requests/new?ticket_form_id=316807")

set(installer.name "nxwitness")
set(android.packageName "com.networkoptix.nxwitness")
set(android.oldPackageName "com.networkoptix.hdwitness")
set(android.alias "hdwitness")
set(android.storepass "hYCmvTDu")
set(android.keypass "31O2zNNy")

# Custom Installer (if needed). If not leave "default"
set(customization.upgradeCode "{ce572797-45bf-4f1c-a783-369ea79c597e}")
set(customization.clientUpgradeCode "{6ae0699e-af9d-4cc1-85a3-9bf8819563af}")
set(customization.serverUpgradeCode "{b4582e3d-c6e1-4f51-806b-ad69417a2584}")

set(customization.clientBundleUpgradeCode "{4505af3c-b20c-4ae9-b5c4-c1fddbc5db34}")
set(customization.serverBundleUpgradeCode "{4904f26d-6146-45f4-be37-80a31c15872d}")
set(customization.fullBundleUpgradeCode "{2c83e785-23e4-4b70-be6c-ed49fa329bb5}")

set(nxtool.company.name "Nx")
set(nxtool.bundleUpgradeCode "{222b19dc-f75f-4096-9cf2-807e309ce06b}")

set(deb.customization.company.name "networkoptix")

# Paxton section
set(paxtonLibraryName "NetworkOptix.NxWitness")
set(paxtonProductId "{234bf98e-b18c-4798-9cb5-a0e90269ef9f}")
set(paxtonProductUpgradeCode "{44777db8-96b1-4b09-9746-6d0390288c84}")
set(paxtonBundleUpgradeCode "{f72abad4-21ca-4c1f-845e-fe437f2859e4}")

set(ios.new_bundle_identifier "com.networkoptix.NxMobile")
set(ios.group_identifier "group.com.networkoptix.NxMobile")
set(ios.sign.identity "iPhone Distribution: Network Optix, Inc. (L6FE34GJWM)")
set(ios.old_app_appstore_id "id648369716")

set(mac.sign.identity "Developer ID Application: Network Optix, Inc. (L6FE34GJWM)")
set(mac.app.sign.identity "3rd Party Mac Developer Application: Network Optix, Inc. (L6FE34GJWM)")
set(mac.pkg.sign.identity "3rd Party Mac Developer Installer: Network Optix, Inc. (L6FE34GJWM)")
set(mac.bundle.identifier "com.networkoptix.HDWitness2")
set(mac.protocol_handler_bundle.identifier "com.networkoptix.protocol_handler")

# Other customizations.
set(freeLicenseIsTrial true)
set(freeLicenseCount 4)
set(freeLicenseKey "0000-0000-0000-0005")
