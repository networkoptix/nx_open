import qbs

GenericProduct
{
    name: "cloud_db_client"

    Depends { name: "nx_network" }

    cpp.includePaths: ["src", "src/include"]

    Export
    {
        Depends { name: "cpp" }
        Depends { name: "nx_network" }

        cpp.includePaths: ["src/include"]
    }
}
