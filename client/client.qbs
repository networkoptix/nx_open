import qbs

Project
{
    condition: withDesktopClient || withMobileClient

    references: [
        "libclient_core",
        "nx_client_desktop",
        "desktop_client",
        "applauncher",
        "mobile_client"
    ]
}
