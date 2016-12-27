import qbs

Project
{
    condition: withMediaServer

    references: [
        "genericrtspplugin",
        "image_library_plugin",
        "mjpg_link"
    ]
}
