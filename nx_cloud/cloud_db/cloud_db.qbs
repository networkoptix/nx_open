import qbs

GenericProduct
{
    name: "cloud_db"
    type: "application"

    condition: project.withCloud

    Depends { name: "libcloud_db" }
}
