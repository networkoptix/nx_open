set (common.guid "{7f2f48c9-e466-45d9-a9a0-1dcfb486f6cb}")
set (nxemail.guid "{0034cf9a-0454-475d-9b54-87d4b4a6b65b}")
set (appserver2.guid "{16bb7b01-5096-4cee-93ae-b34b3153459f}")
set (axiscamplugin.guid "{c27722d6-d3ae-41dc-9467-52d93f5b119a}")
set (evidence_plugin.guid "{061d1513-9a37-4d14-90f5-11d42d2d7827}")
set (genericrtspplugin.guid "{7fafaf1d-0d3d-42ea-aa11-4ffce0f170f9}")
set (image_library_plugin.guid "{62b24237-9c9d-4900-b32d-aa678317b4ba}")
set (isd_native_plugin.guid "{9d3f6a6f-ec63-4269-bf34-cc1060f80e90}")
set (it930x_plugin.guid "{2c75d1b9-a6f1-4aa6-9d7e-8c0e602bd8fd}")
set (mjpg_link.guid "{c8a846fd-d881-45d8-9f6c-c3e02eaf5763}")
set (quicksyncdecoder.guid "{84ca6c5b-525e-438a-940c-50c14db49c66}")
set (rpi_cam.guid "{e9a0aea0-451a-497b-b5a2-ec649c9419c2}")
set (xvbadecoder.guid "{9b453bd1-92cb-4235-a6d4-2d77879f76fa}")
set (mediaserver_core.guid "{821e5e4a-e882-4eec-aa02-71c9c1ff35b9}")
set (mediaserver.guid "{ee61c3cc-791c-4f9f-9917-9e7c9ce9e907}")
set (mobile_client.guid "{4d58c9a4-8dbe-480c-a974-634a7e2862ef}")
set (applauncher.guid "{75b8ee80-e089-4de2-b7a1-d1464a9b4351}")
set (client.core.guid "{7b7822da-47c1-4758-a07f-c2edcfc63d66}")
set (client.bin.guid "{6de071a7-831c-43a1-88ae-ee9e12cd4050}")
set (client.guid "{6a4121d4-b02f-4e35-b941-ca9a9a2efb69}")
set (axclient.guid "{d3f564c6-315b-426b-8ed3-890d91300374}")
set (traytool.guid "{e2bc6f17-8fc1-4aeb-b614-3fe2c74c41f6}")
set (nxtool.guid "{d78c3988-0459-4dce-a1dc-e70df990fa54}")
set (ftpstorageplugin.guid "{f6c9f457-910c-4c0d-bb67-b4f752a8664e}")
set (storage_plugin_test_client.guid "{83e8bbad-6976-45da-bdd7-baf8f17fc71c}")

set (updatefeed.auto "false")
set (mac.skip.sign "false")
set (windows.skip.sign "false")
#skip.sign false
set (help.language "english")
set (quicksync "false")
set (dynamic.customization "false")
set (display.mobile.name "${display.product.name} Mobile")
# if true, you can use --customization <path> to specify customization
# Note: config json files should be there

set (default.skin "DarkSkin")
set (background.cycle "120")
set (uri.protocol "vms")

set (nxtoolUpgradeCode "cc740987-5070-4750-b853-327417176031")
set (nxtool.company.name "${company.name}")
set (company.support.link "${company.support.address}")

# Help and settings URLs
set (helpUrl "http://networkoptix.com/files/help")
set (settings.url "http://networkoptix.com/files/settings")
set (showcase.url "http://networkoptix.com/files/showcase")
set (update.generator.url "http://updates.hdw.mx/upcombiner/upcombine")

# Update parameters
set (mirrorListUrl "http://downloads.hdwitness.com/clients/mirror.xml")
set (prodUpdateFeedUrl "http://updates.hdwitness.com/updates.json")
set (releaseNotesUrl "http://updates.hdwitness.com/releasenotes.json")
set (testUpdateFeedUrl "http://updates.hdwitness.com/updates.json")
# Additional Features
set (vmax "false")
#(true or false)

set (ios.minimum_os_version "6.0")
set (ios.playButton.tint "#00A1D4")

#VMS-1672: Sometimes unauthorized server is not visible in the servers list
set (nxec.ec2ProtoVersion "2505")

set (old.android.packagename "${namespace.major}.${namespace.minor}.${namespace.additional}")
set (new.android.packagename "${namespace.major}.${namespace.minor}.${new.namespace.additional}")
