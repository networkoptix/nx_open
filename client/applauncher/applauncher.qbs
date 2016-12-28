import qbs

GenericProduct
{
    name: "applauncher"
    type: "application"

    condition: project.withDesktopClient

    Depends { name: "nx_network" }
    Depends { name: "qtsinglecoreapplication" }

    Properties
    {
        condition: qbs.targetOS.contains("macos")
        cpp.frameworks: ["Foundation"]
    }

    Group
    {
        files: "maven/filter-resources/applauncher_app_info_impl.cpp"
        fileTags: "configure.input"
        configure.outputTags: "cpp"
        configure.replacements: ({
            "product.title": customization.companyName + " "
                + customization.productDisplayName + " Launcher",
            "client.binary.name": vms.clientBinaryName,
            "mirrorListUrl": customization.mirrorListUrl,
            "installation.root": vms.installationRoot,
            "display.product.name": customization.productDisplayName
        })
    }
}
