import qbs

GenericProduct
{
    name: "nx_audio"

    Depends { name: "common" }
    Depends
    {
        name: "openal"
        condition: qbs.targetOS.contains("windows") || qbs.targetOS.contains("android")
    }

    Properties
    {
        condition: qbs.targetOS.contains("linux")
        cpp.dynamicLibraries: ["openal"]
    }
    Properties
    {
        condition: qbs.targetOS.contains("darwin")
        cpp.frameworks: ["OpenAL"]
    }

    Export
    {
        Depends { name: "common" }
        Depends
        {
            name: "openal"
            condition: qbs.targetOS.contains("windows") || qbs.targetOS.contains("android")
        }
    }
}
