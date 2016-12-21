import qbs

GenericProduct
{
    name: "nx_audio"

    Depends { name: "common" }

    Properties
    {
        condition: qbs.targetOS.contains("linux")
        cpp.dynamicLibraries: ["openal"]
    }

    Export
    {
        Depends { name: "common" }
    }
}
