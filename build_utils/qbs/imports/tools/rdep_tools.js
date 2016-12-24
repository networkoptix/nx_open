var TextFile = loadExtension("qbs.TextFile")
var FileInfo = loadExtension("qbs.FileInfo")
var File = loadExtension("qbs.File")
var Process = loadExtension("qbs.Process")
var Environment = loadExtension("qbs.Environment")

var pythonExecutable = "python2"
var syncScript = "/home/kdsnake/develop/nx_build/build_utils/python/sync.py"
var packagesDir = FileInfo.joinPaths(Environment.getEnv("environment"), "packages")

function fullPackageName(target, packageName, version)
{
    return target + "/" + packageName + (version ? "-" + version : "")
}

function versionVariable(packageName)
{
    return packageName.replace("-", "_") + "Version"
}

function logDependency(fullPackageName)
{
    console.info(">> Probing [" + fullPackageName + "]")
}

function getAbsolutePaths(packageDir, pathList)
{
    if (!pathList)
        return undefined

    var result = []
    for (var i = 0; i < pathList.length; ++i)
    {
        var path = pathList[i]
        if (path.startsWith("/"))
        {
            result.push(path)
            continue
        }

        var absPath = FileInfo.joinPaths(packageDir, path)
        if (File.exists(absPath))
            result.push(absPath)
    }

    return result
}

function toArray(value)
{
    return Array.isArray(value) ? value : [vlaue]
}

function substituteVar(str, variable, value)
{
    return str.replace("$" + variable, value)
}

function substituteArray(list, variable, value)
{
    var result = []

    for (var i = 0; i < list.length; ++i)
        result.push(substituteVar(list[i], variable, value))

    return result
}

function loadPackageInfo(packageDir, infoFileName)
{
    var infoFilePath = FileInfo.joinPaths(packageDir, infoFileName)
    if (!File.exists(infoFilePath))
        return null

    var f = new TextFile(infoFilePath)
    var data = f.readAll()
    f.close()

    var info = JSON.parse(data)

    if (info.type == "cpp")
    {
        for (prop in info)
        {
            var value = info[prop]
            info[prop] = Array.isArray(value)
                ? substituteArray(value, "PWD", packageDir)
                : substituteVar(value, "PWD", packageDir)
        }

        var includePaths = ["include"]
        if (info.includePaths)
            includePaths = toArray(info.includePaths)
        info.includePaths = getAbsolutePaths(packageDir, includePaths)

        var libraryPaths = ["lib"]
        if (info.libraryPaths)
            libraryPaths = toArray(info.libraryPaths)
        info.libraryPaths = getAbsolutePaths(packageDir, libraryPaths)
    }

    return info
}
