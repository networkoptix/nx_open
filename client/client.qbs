import qbs

Project
{
    condition: withDesktopClient || withMobileClient

    references: [
        "libclient_core",
        "libclient",
        "desktop_client",
        "applauncher",
        "mobile_client"
    ]
}
