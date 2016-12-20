import qbs

GenericProduct
{
    name: "nx_media"

    Depends { name: "Qt.multimedia-private" }
    Depends { name: "nx_audio" }
    Depends { name: "nx_streaming" }

    Export
    {
        Depends { name: "nx_audio" }
        Depends { name: "nx_streaming" }
    }
}
