import qbs
import VmsUtils

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
        if (qbs.targetOS.contains("macos"))
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
        if (qbs.targetOS.contains("macos"))
            return "/Applications/"
        return "/opt/" + customization.depCompanyName
    }

    property string updateFeedUrl: project.beta
        ? customization.testUpdateFeedUrl
        : customization.prodUpdateFeedUrl

    property string arch: VmsUtils.currentArch()
    property string platform: VmsUtils.currentPlatform()
    property string modification:
    {
        if (project.box)
            return project.box

        if (platform == "windows")
            return "winxp"
        if (platform == "linux")
            return "ubuntu"
        if (platform == "macosx")
            return "none"
        if (platform == "android" || platform == "ios")
            return "none"

        throw "Cannot detect modification for platform " + platform
    }
}
