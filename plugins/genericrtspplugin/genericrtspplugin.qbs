import qbs

GenericProduct
{
    name: "genericrtspplugin"

    Depends { name: "nx_streaming" }

    Export
    {
        Depends { name: "nx_streaming" }
    }
}
