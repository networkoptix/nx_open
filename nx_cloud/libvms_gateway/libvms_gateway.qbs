import qbs

GenericProduct
{
    name: "libvms_gateway"
    targetName: "vms_gateway"
    condition: project.withClouds

    Depends { name: "cloud_db_client" }
    Depends { name: "qtservice" }
    Depends { name: "qtsinglecoreapplication" }

    Properties
    {
        condition: qbs.targetOS.contains("windows")
        cpp.defines: ["_VARIADIC_MAX=8"]
    }

    Group
    {
        files: "maven/filter-resources/libvms_gateway_app_info_impl.cpp"
        fileTags: "configure.input"
        configure.outputTags: "cpp"
        configure.replacements: ({
            "product.title": customization.productName + " libvms_gateway",
            "product.display.title": customization.productDisplayName + " VMS Gateway"
        })
    }

    ResourcesGroup
    {
        resources.resourceSourceBase: product.sourceDirectory + "/static-resources"
    }

    Export
    {
        Depends { name: "cloud_db_client" }
        Depends { name: "qtservice" }
        Depends { name: "qtsinglecoreapplication" }
    }
}
