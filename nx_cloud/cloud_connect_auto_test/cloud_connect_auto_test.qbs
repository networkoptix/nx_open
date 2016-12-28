import qbs

GenericProduct
{
    name: "cloud_connect_auto_test"
    type: "application"

    builtByDefault: false

    Depends { name: "nx_network" }
}
