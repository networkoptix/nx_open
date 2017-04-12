import qbs

GenericProduct
{
    name: "nx_vms_utils"

    Depends { name: "nx_utils" }

    cpp.defines: ["NX_VMS_UTILS_API=" + vms_cpp.apiExportMacro]

    Group
    {
        files: "maven/filter-resources/nx_vms_utils_app_info_impl.cpp"
        fileTags: "configure.input"
        configure.outputTags: "cpp"
        configure.replacements: ({
            "uri.protocol": customization.uriProtocol,
            "client.display.name": customization.productDisplayName + " Client",
            "installer.name": customization.installerName
        })
    }

    Export
    {
        Depends { name: "cpp" }
        Depends { name: "nx_utils" }

        cpp.defines: ["NX_VMS_UTILS_API=" + vms_cpp.apiImportMacro]
    }
}
