import qbs

GenericProduct
{
    name: "time_server"
    type: "application"

    builtByDefault: false

    Depends { name: "nx_network" }

    Group
    {
        files: "maven/filter-resources/time_server_app_info_impl.cpp"
        fileTags: "configure.input"
        configure.outputTags: "cpp"
        configure.replacements: ({
            "product.title": customization.companyName + " Time Server",
            "product.display.title": customization.companyName + " Time Server"
        })
    }
}
