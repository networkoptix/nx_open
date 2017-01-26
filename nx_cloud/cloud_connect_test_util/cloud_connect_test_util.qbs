import qbs

GenericProduct
{
    name: "cloud_connect_test_util"
    type: "application"

    builtByDefault: false

    Depends { name: "nx_network" }
}
