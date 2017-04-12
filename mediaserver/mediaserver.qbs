import qbs

Project
{
    condition: withMediaServer

    BranchProbe
    {
        id: branchProbe
    }

    RdepProbe
    {
        id: serverExternalForBranch
        packageName: "server-external-" + branchProbe.branch
        target: "any"
        required: false
    }
    RdepProbe
    {
        id: serverExternal
        packageName: "server-external-" + releaseVersion
        target: "any"
        condition: !serverExternalForBranch.found
    }

    references: [
        serverExternalForBranch.found
            ? serverExternalForBranch.packagePath
            : serverExternal.packagePath
    ]

    GenericProduct
    {
        name: "mediaserver"
        type: "application"

        Depends { name: "festival" }
        Depends { name: "server-external" }
        Depends { name: "mediaserver_core" }

        ResourcesGroup
        {
            resources.resourceSourceBase: product.sourceDirectory + "/static-resources"
        }
    }
}
