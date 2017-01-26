import qbs

GenericProduct
{
    name: "udt"

    Properties
    {
        condition: qbs.targetOS.contains("unix")
        cpp.dynamicLibraries: ["pthread"]
    }

    cpp.defines: ["UDT_API=" + vms_cpp.apiExportMacro]

    Export
    {
        Depends { name: "cpp" }
        cpp.defines: ["UDT_API=" + vms_cpp.apiImportMacro]
    }
}
