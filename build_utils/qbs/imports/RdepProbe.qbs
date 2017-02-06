import qbs
import Rdep

Probe
{
    // imput properties
    property string packageName
    property string target
    property bool required: true

    // output properties
    property string packagePath

    // internal
    property var _project: project

    configure: {
        if (!packageName)
            throw "Package name must be set"

        var rdep = new Rdep.Rdep(_project)
        packagePath = required
            ? rdep.sync(packageName, target)
            : rdep.trySync(packageName, target)
        if (packagePath)
            found = true
    }
}
