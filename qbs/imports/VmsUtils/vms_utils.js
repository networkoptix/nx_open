function currentPlatform()
{
    if (qbs.targetOS.contains("android"))
        return "android"
    if (qbs.targetOS.contains("linux"))
        return "linux"
    if (qbs.targetOS.contains("windows"))
        return "windows"
    if (qbs.targetOS.contains("ios"))
        return "ios"
    if (qbs.targetOS.contains("macos"))
        return "macosx"

    throw "Unsupported target OS " + qbs.targetOS
}

function currentArch()
{
    if (qbs.architecture == "x86_64")
        return "x64"
    if (qbs.architecture == "x86")
        return qbs.architecture
    if (qbs.architecture.startsWith("arm"))
        return "arm"

    throw "Unsupported target architecture " + qbs.architecture
}

function currentTarget(box)
{
    if (box)
        return box

    var platform = currentPlatform()
    var arch = currentArch()

    var target = platform + "-" + arch
    if (platform == "ios")
        target = "ios"

    var supportedTargets = [
        "linux-x64",
        "linux-x86",
        "windows-x64",
        "windows-x86",
        "macosx-x64",
        "android-arm",
        "ios",
        "rpi",
        "bpi",
        "bananapi",
        "tx1",
        "isd",
        "isd_s2"
    ]

    if (!supportedTargets.contains(target))
    {
        if (target == "linux-arm")
        {
            throw "Unsupported target linux-arm."
            " Probably you forgot to specify project.box parameter."
        }

        throw "Unsupported target " + target
    }

    return target
}
