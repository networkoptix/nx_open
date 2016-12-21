import qbs

GenericProduct
{
    name: "libcloud_db"
    targetName: "cloud_db"

    Depends { name: "cloud_db_client" }
    Depends { name: "appserver2" }
    Depends { name: "nx_email" }

    cpp.includePaths: base.concat([project.sourceDirectory + "/nx_cloud"])

    Properties
    {
        condition: qbs.targetOS.contains("windows")
        cpp.defines: ["_VARIADIC_MAX=8"]
    }

    Group
    {
        files: "maven/filter-resources/libcloud_db_app_info_impl.cpp"
        fileTags: "configure.input"
        configure.outputTags: "cpp"
        configure.replacements: ({
            "product.title": customization.companyName + " libcloud_db",
            "product.display.title": customization.companyName + " Cloud Database"
        })
    }

    ResourcesGroup
    {
        resources.resourceSourceBase: product.sourceDirectory + "/static-resources"
    }

    Export
    {
        Depends { name: "cloud_db_client" }
        Depends { name: "appserver2" }
        Depends { name: "nx_email" }
    }
}
