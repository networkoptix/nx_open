set(updatefeed.auto "false")
set(mac.skip.sign "false")
set(windows.skip.sign "false")

set(help.language "english")
set(quicksync "false")
set(dynamic.customization "false")
set(display.mobile.name "${display.product.name} Mobile")

set(uri.protocol "nx-vms")

set(nxtoolUpgradeCode "cc740987-5070-4750-b853-327417176031")
set(build_nxtool OFF)

set(backgroundImage "{}")
set(defaultWebPages "{}")
set(defaultSkin "dark_blue")

set(freeLicenseCount "4")
set(freeLicenseIsTrial "true")

# Update parameters
set(update.generator.url "http://updates.hdw.mx/upcombiner/upcombine")
set(mirrorListUrl "http://downloads.hdwitness.com/clients/mirror.xml")
set(prodUpdateFeedUrl "http://updates.hdwitness.com/updates.json")
set(releaseNotesUrl "http://updates.hdwitness.com/releasenotes.json")
set(testUpdateFeedUrl "http://updates.hdwitness.com/updates.json")

if(beta)
    set(updateFeedUrl ${testUpdateFeedUrl})
else()
    set(updateFeedUrl ${prodUpdateFeedUrl})
endif()

set(mac_certificate_file_password "qweasd123")

# Additional Features
set(vmax false)
# if true, you can use --customization=<path> to specify customization
set(dynamic.customization false)

# Localization
set(additionalTranslations)
set(installer.language "en_US")
set(installer.cultures "en-us")

set(customization.defines "")
