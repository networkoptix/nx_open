import qbs

GenericProduct
{
    name: "nx_fusion"

    cpp.defines: ["NX_FUSION_API=" + vms_cpp.apiExportMacro]
    cpp.includePaths: ["src", project.sourceDirectory + "/common/src"]

    Depends { name: "Qt.gui"}
    Depends { name: "nx_utils" }

    Export
    {
        Depends { name: "cpp" }
        Depends { name: "Qt.gui"}
        Depends { name: "nx_utils" }

        cpp.defines: ["NX_FUSION_API=" + vms_cpp.apiImportMacro]
    }
}
