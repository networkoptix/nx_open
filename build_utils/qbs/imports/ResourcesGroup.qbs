import qbs

Group
{
    files: "*"
    fileTags: ["resources.resource_data"]
    prefix: resources.resourceSourceBase + "/**/"
}
