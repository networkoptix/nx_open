import qbs

GenericProduct
{
    name: "nx_streaming"

    Depends { name: "common" }

    cpp.includePaths: ["src", project.sourceDirectory + "/common_libs/nx_network/src"]
    cpp.allowUnresolvedSymbols: true
    cpp.defines: ["NX_NETWORK_API=" + vms_cpp.apiImportMacro]

    Export
    {
        Depends { name: "cpp" }
        Depends { name: "common" }

        Properties
        {
            condition: qbs.targetOS.contains("android")
            cpp.libraryPaths: [product.buildDirectory]
        }
    }
}
