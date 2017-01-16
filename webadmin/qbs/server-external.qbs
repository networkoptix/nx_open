import qbs 1.0

Product
{
    name: "server-external"

    Group
    {
        files: ["bin/external.dat"]
        qbs.install: true
        qbs.installDir: "bin"
    }
}
