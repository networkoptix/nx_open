import qbs

GenericProduct
{
    name: "connection_mediator"
    type: "application"

    Depends { name: "libconnection_mediator" }
}
