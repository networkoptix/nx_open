import qbs

Project
{
    condition: withInstallers

    references: [
        "client-deb"
    ]
}
