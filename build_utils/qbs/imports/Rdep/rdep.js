var FileInfo = loadExtension("qbs.FileInfo")
var Process = loadExtension("qbs.Process")

function versionVariable(packageName)
{
    return packageName.replace("-", "_") + "Version"
}

function fullPackageName(target, packageName, version)
{
    return target + "/" + packageName + (version ? "-" + version : "")
}

var Rdep = (function()
{
    function Rdep(project)
    {
        this.project = project
        this.packagesDir = project.packagesDir
        this.python = project.python
        this.target = project.target
        this.syncScript = project.sourceDirectory + "/build_utils/python/sync.py"
    }

    Rdep.prototype.__sync = function(packageName, target, version, optional)
    {
        if (!target)
            target = this.target

        if (!version)
            version = this.project[versionVariable(packageName)]

        var pack = target + "/" + packageName + (version ? "-" + version : "")

        console.info("Synching " + pack)

        var process = new Process()
        if (process.exec(this.python, [this.syncScript, pack], false) !== 0)
        {
            if (!optional)
                throw "Cannot sync package " + pack
            return
        }

        return FileInfo.joinPaths(this.packagesDir, pack)
    }

    Rdep.prototype.sync = function(packageName, target, version)
    {
        return this.__sync(packageName, target, version, false)
    }

    Rdep.prototype.trySync = function(packageName, target, version)
    {
        return this.__sync(packageName, target, version, true)
    }

    return Rdep
})()
