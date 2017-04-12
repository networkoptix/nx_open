import qbs

GenericProduct
{
    name: "libconnection_mediator"
    targetName: "connection_mediator"
    condition: project.withClouds

    Depends { name: "cloud_db_client" }
    Depends { name: "nx_network" }
    Depends { name: "qtservice" }
    Depends { name: "qtsinglecoreapplication" }

    Group
    {
        files: "maven/filter-resources/libconnection_mediator_app_info_impl.cpp"
        fileTags: "configure.input"
        configure.outputTags: "cpp"
        configure.replacements: ({
            "product.title": customization.companyName + " Mediator Lib",
            "product.display.title": customization.companyName + " Mediator Lib"
        })
    }

    Export
    {
        Depends { name: "cloud_db_client" }
        Depends { name: "nx_network" }
        Depends { name: "qtservice" }
        Depends { name: "qtsinglecoreapplication" }
    }
}
