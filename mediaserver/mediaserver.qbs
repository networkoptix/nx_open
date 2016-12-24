import qbs

GenericProduct
{
    name: "mediaserver"
    type: "application"

    Depends { name: "server-external" }
    Depends { name: "mediaserver_core" }

    ResourcesGroup
    {
        resources.resourceSourceBase: product.sourceDirectory + "/static-resources"
    }
}
