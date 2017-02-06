import qbs

GenericProduct
{
    name: "connection_mediator"
    type: "application"
    condition: project.withClouds

    Depends { name: "libconnection_mediator" }
}
