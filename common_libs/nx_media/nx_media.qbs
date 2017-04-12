import qbs

Project
{
    RdepProbe
    {
        id: jpegTurbo
        packageName: "libjpeg-turbo"
        condition: qbs.targetOS.contains("android") || qbs.targetOS.contains("ios")
    }

    references: jpegTurbo.found ? jpegTurbo.packagePath : []

    GenericProduct
    {
        name: "nx_media"

        Depends { name: "Qt.multimedia-private" }
        Depends { name: "Qt.androidextras"; condition: qbs.targetOS.contains("android") }
        Depends { name: "nx_audio" }
        Depends { name: "nx_streaming" }
        Depends { name: "libjpeg-turbo"; condition: jpegTurbo.found }

        Export
        {
            Depends { name: "Qt.androidextras"; condition: qbs.targetOS.contains("android") }
            Depends { name: "nx_audio" }
            Depends { name: "nx_streaming" }
            Depends { name: "libjpeg-turbo"; condition: jpegTurbo.found }
        }
    }
}
