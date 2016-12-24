import qbs

GenericProduct
{
    name: "cloud_db"
    type: "application"

    Depends { name: "libcloud_db" }
}
