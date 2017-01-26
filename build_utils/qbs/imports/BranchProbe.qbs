import qbs
import qbs.File
import qbs.TextFile

Probe
{
    id: branchProbe

    property string branch

    property string _branchFile: project.sourceDirectory + "/.hg/branch"
    property var _lastModified: File.lastModified(_branchFile)

    configure:
    {
        branch = (new TextFile(_branchFile)).readAll().trim()
    }
}
