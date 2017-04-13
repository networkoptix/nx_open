import qbs

Project
{
    condition: withDesktopClient

    RdepProbe
    {
        id: help
        packageName: "help"
        target: "any"
    }

    references: [
        help.packagePath
    ]

    GenericProduct
    {
        name: "desktop_client"
        type: "application"

        Depends { name: "nx_client_desktop" }
        Depends { name: "qtsingleapplication" }

        Depends { name: "festival" }
        Depends { name: "festival-vox" }
        Depends { name: "roboto-ttf" }
        Depends { name: "roboto-mono-ttf" }
        Depends { name: "help" }

        Export
        {
            Depends { name: "nx_client_desktop" }
            Depends { name: "festival" }
            Depends { name: "festival-vox" }
            Depends { name: "roboto-ttf" }
            Depends { name: "roboto-mono-ttf" }
            Depends { name: "help" }
        }
    }
}
