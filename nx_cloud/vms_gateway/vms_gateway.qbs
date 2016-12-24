import qbs

GenericProduct
{
    name: "vms_gateway"
    type: "application"

    Depends { name: "libvms_gateway" }
}
