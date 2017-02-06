import qbs

GenericProduct
{
    name: "libclient_core"
    targetName: "client_core"

    Depends
    {
        name: "Qt"
        submodules: ["quick", "quick-private", "qtmultimediaquicktools-private"]
    }
    Depends { name: "appserver2" }
    Depends { name: "nx_vms_utils" }
    Depends { name: "cloud_db_client" }
    Depends { name: "nx_media" }

    Export
    {
        Depends
        {
            name: "Qt"
            submodules: ["quick", "quick-private", "qtmultimediaquicktools-private"]
        }
        Depends { name: "appserver2" }
        Depends { name: "nx_vms_utils" }
        Depends { name: "cloud_db_client" }
        Depends { name: "nx_media" }
    }
}
