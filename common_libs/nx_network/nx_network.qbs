import qbs

GenericProduct
{
    name: "nx_network"

    cpp.defines: ["NX_NETWORK_API=" + vms_cpp.apiExportMacro]

    Depends { name: "nx_streaming" }
    Depends { name: "udt" }

    Export
    {
        Depends { name: "cpp" }
        Depends { name: "nx_streaming" }
        Depends { name: "udt" }

        cpp.defines: ["NX_NETWORK_API=" + vms_cpp.apiImportMacro]

        Properties
        {
            condition: qbs.targetOS.contains("android")
            cpp.libraryPaths: [product.buildDirectory]
        }
    }
}
