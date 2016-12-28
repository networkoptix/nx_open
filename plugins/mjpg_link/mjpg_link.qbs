import qbs

GenericProduct
{
    name: "mjpg_link"

    Depends { name: "nx_network" }

    Export
    {
        Depends { name: "nx_network" }
    }
}
