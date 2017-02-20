import qbs

GenericProduct
{
    name: "appserver2"

    Depends { name: "nx_network" }
    Depends { name: "cloud_db_client" }

    ResourcesGroup
    {
        resources.resourceSourceBase: product.sourceDirectory + "/static-resources"
    }

    Export
    {
        Depends { name: "nx_network" }
        Depends { name: "cloud_db_client" }
    }
}
