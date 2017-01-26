import qbs

GenericProduct
{
    name: "nx_email"

    Depends { name: "nx_network" }

    Export
    {
        Depends { name: "nx_network" }
    }
}
