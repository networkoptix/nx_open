set(build_nxtool ON)
set(build_paxton ON)
set(enable_hanwha true)

set(defaultSkin "dark_blue")

# Components Namings
set(display.product.name "Nx MetaVMS")
set(display.mobile.name "Nx Mobile")
set(product.name "Nx MetaVMS")
set(product.name.short "metavms")
set(product.appName "Nx MetaVMS")
set(company.name "Network Optix")
set(liteDeviceName "nx1")
set(cloudName "Nx Meta Cloud")
set(uri.protocol "nx-vms")

# Default settings
set(backgroundImage
    "{\"enabled\": true, \"name\": \":/skin/background.png\", \"mode\": \"Crop\"}")
set(defaultWebPages
    "{\"Home Page\": \"http://networkoptix.com\", \"Support\": \"http://support.networkoptix.com\"}")

# Support section
set(companyUrl "http://networkoptix.com")
set(productUrl "http://networkoptix.com/nxwitness-overview")
set(supportUrl "https://networkoptix.com/meta/")
set(supportEmail "meta@networkoptix.com")
set(supportPhone "")
set(licenseEmail "meta@networkoptix.com")

set(installer.name "metavms")
set(android.packageName "com.networkoptix.nxwitness")
set(android.oldPackageName "com.networkoptix.hdwitness")
set(android.alias "hdwitness")
set(android.storepass "hYCmvTDu")
set(android.keypass "31O2zNNy")

# Custom Installer (if needed). If not leave "default"
set(customization.upgradeCode "{5c6f8f50-99d3-4bd5-9201-08e5c375bb13}")
set(customization.clientUpgradeCode "{0f2ed89b-8c56-4a6c-a858-59007eb004c0}")
set(customization.serverUpgradeCode "{8ac4ddb5-8ebe-423f-ae52-5df4ef9e5f20}")

set(customization.clientBundleUpgradeCode "{3401937b-5715-40f7-bf3d-d34b67b3b317}")
set(customization.serverBundleUpgradeCode "{b0f6cba5-66f5-4ecd-bba5-87064c7618a6}")
set(customization.fullBundleUpgradeCode "{b98a8e7a-2e21-472c-b951-e16f82a43798}")

set(nxtool.company.name "Nx")
set(nxtool.bundleUpgradeCode "{37d2fd11-7e22-43f3-ae84-91342168257d}")

set(deb.customization.company.name "networkoptix")

# Paxton section
set(paxton.className "NxWitness")
set(paxton.classId "{930BF2FA-8BEB-4975-A04E-7FD63A4825AB}")
set(paxton.interfaceId "{5C0BD575-9376-4CF0-B34F-044AC58BD8AB}")
set(paxton.eventsId "{F220AAEC-AE9E-4771-9397-09F6F00C3B89}")
set(paxton.typeLibId "{534EA078-DCAF-4EF0-B982-95B5656D87B3}")
set(paxton.appId "{B21774A2-4417-4F07-84DC-37439C9B1063}")
set(paxton.upgradeCode "{44777DB8-96B1-4B09-9746-6D0390288C84}")
set(paxton.bundleUpgradeCode "{f72abad4-21ca-4c1f-845e-fe437f2859e4}")

set(ios.new_bundle_identifier "com.networkoptix.NxMobile")
set(ios.group_identifier "group.com.networkoptix.NxMobile")
set(ios.sign.identity "iPhone Distribution: Network Optix, Inc. (L6FE34GJWM)")
set(ios.old_app_appstore_id "id648369716")

set(mac.skip.sign false)
set(mac.sign.identity "Developer ID Application: Network Optix, Inc. (L6FE34GJWM)")
set(mac.app.sign.identity "3rd Party Mac Developer Application: Network Optix, Inc. (L6FE34GJWM)")
set(mac.pkg.sign.identity "3rd Party Mac Developer Installer: Network Optix, Inc. (L6FE34GJWM)")
set(mac.bundle.identifier "com.networkoptix.metavms")
set(mac.protocol_handler_bundle.identifier "com.networkoptix.protocol_handler")

# Other customizations.
set(freeLicenseIsTrial true)
set(freeLicenseCount 4)
set(freeLicenseKey "0000-0000-0000-0029")

# Installer Customizations
set(sign.password "qweasd123")
set(sign.cer "app.p12")

# Localization
set(defaultTranslation en_US)
set(additionalTranslations
    en_GB
    fr_FR
    de_DE
    ru_RU
    es_ES
    ja_JP
    ko_KR
    tr_TR
    zh_CN
    zh_TW
    he_IL
    hu_HU
    nl_NL
    pl_PL
    vi_VN
    th_TH
)
