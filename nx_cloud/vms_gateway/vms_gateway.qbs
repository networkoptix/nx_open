import qbs

GenericProduct
{
    name: "vms_gateway"
    type: "application"
    condition: project.withClouds

    Depends { name: "libvms_gateway" }
}
