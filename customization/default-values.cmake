set(updatefeed.auto "false")
set(mac.skip.sign "false")
set(windows.skip.sign "false")

set(help.language "english")
set(quicksync "false")
set(dynamic.customization "false")
set(display.mobile.name "${display.product.name} Mobile")

set(uri.protocol "nx-vms")

set(nxtoolUpgradeCode "cc740987-5070-4750-b853-327417176031")
set(short.company.name "${company.name}")
set(backgroundImage "{}")
set(defaultWebPages "{}")

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

# Additional Features
set(vmax "false")

set(customization.defines "")
set(compatibleCustomizations)
