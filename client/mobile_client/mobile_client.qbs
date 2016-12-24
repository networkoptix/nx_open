import qbs
import qbs.FileInfo

GenericProduct
{
    name: "mobile_client"
    type: "application"

    Depends { name: "Qt"; submodules: ["webchannel", "websockets"] }
    Depends { name: "qtsingleguiapplication" }
    Depends { name: "roboto-ttf" }
    Depends { name: "libclient_core" }

    configure.replacements: ({
        "product.title": customization.companyName + " "
            + customization.productDisplayName + " Mobile Client",
        "product.display.title": customization.productDisplayName + " Mobile Client",
        "liteMode": vms.box == "bpi",
        "android.oldPackageName": customization.androidOldPackageName,
        "ios.old_app_appstore_id" : customization.iosOldAppstoreId
    })

    Group
    {
        files: "maven/filter-resources/mobile_client_app_info_impl.cpp"
        fileTags: "configure.input"
        configure.outputTags: "cpp"
    }

    ResourcesGroup
    {
        resources.resourceSourceBase: product.sourceDirectory + "/static-resources"
    }
}
