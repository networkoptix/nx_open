import qbs

GenericProduct
{
    name: "image_library_plugin"

    Depends { name: "Qt.core" }

    cpp.includePaths: [project.sourceDirectory + "/common/src"]

    Export
    {
        Depends { name: "Qt.core" }
    }
}
