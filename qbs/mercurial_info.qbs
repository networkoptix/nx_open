import qbs
import qbs.File
import qbs.TextFile
import qbs.Process

Product
{
    name: "mercurialInfo"

    Probe
    {
        id: infoProbe

        property string changeSet
        property string branch

        readonly property string _projectDir: project.sourceDirectory
        readonly property string _branchFile: _projectDir + "/.hg/branch"
        readonly property var _lastModified: File.lastModified(_branchFile)

        configure:
        {
            branch = (new TextFile(_branchFile)).readAll().trim()

            var process = new Process()
            process.setWorkingDirectory(_projectDir)
            process.exec("hg", ["id", "-i"], true)
            changeSet = process.readStdOut().trim()
        }
    }

    Export
    {
        property string changeSet: infoProbe.changeSet
        property string branch: infoProbe.branch
    }
}
