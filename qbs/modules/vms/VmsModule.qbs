import qbs

Module
{
    Depends { name: "customization" }

    property stringList defaultLibraryType:
    {
        if (qbs.targetOS.contains("android") || qbs.targetOS.contains("windows"))
            return ["staticlibrary"]

        return ["dynamiclibrary"]
    }

    property string clientBinaryName: {
        if (qbs.targetOS.contains("windows"))
            return customization.productName + ".exe"
        if (qbs.targetOS.contains("macosx"))
            return customization.productDisplayName
        return "client-bin"
    }
    property string minilauncherBinaryName:
        qbs.targetOS.contains("windows")
            ? customization.productName + " Launcher.exe"
            : "applauncher"
    property string applauncherBinaryName:
        qbs.targetOS.contains("windows") ? "applauncher.exe" : "applauncher-bin"
    property string launcherVersionFile: "launcher.version"

    property string clientName:
        customization.companyName + " " + customization.productName + " Client"
    property string clientDisplayName: customization.productDisplayName + " Client"

    property string installationRoot: {
        if (qbs.targetOS.contains("windows"))
            return "C:/Program Files/"
                + customization.companyName
                + "/"
                + customization.productDisplayName
                + "/"
        if (qbs.targetOS.contains("macosx"))
            return "/Applications/"
        return "/opt/" + customization.depCompanyName
    }

    property string updateFeedUrl: project.beta
        ? customization.testUpdateFeedUrl
        : customization.prodUpdateFeedUrl

    property string box: "none"
    property string arch:
    {
        if (qbs.architecture == "x86_64")
            return "x64"
        else
            return qbs.architecture
    }
    property string platform:
    {
        if (qbs.targetOS.contains("windows"))
            return "windows"
        else if (qbs.targetOS.contains("android"))
            return "android"
        else if (qbs.targetOS.contains("ios"))
            return "ios"
        else if (qbs.targetOS.contains("linux"))
            return "linux"
        else if (qbs.targetOS.contains("osx"))
            return "macosx"
        return "unknown_platform"
    }
    property string modification:
    {
        if (box != "none")
            return box

        if (platform == "windows")
            return "winxp"
        else if (platform == "linux")
            return "ubuntu"
        return "unknown_box"
    }
}
