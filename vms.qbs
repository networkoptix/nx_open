import qbs
import qbs.Environment
import qbs.File
import qbs.FileInfo
import qbs.TextFile
import qbs.Process

Project
{
    name: "vms"
    minimumQbsVersion: "1.8.0"
    qbsSearchPaths: ["build_utils/qbs", "qbs", "customization/" + customization + "/qbs"]

    property int versionMajor: 3
    property int versionMinor: 0
    property int versionBugfix: 0
    property int buildNumber: 0
    property string releaseVersion:
        versionMajor + "." + versionMinor + "." + versionBugfix
    property string fullVersion:
        versionMajor + "." + versionMinor + "." + versionBugfix + "." + buildNumber

    property bool developerBuild: true
    property string target: "linux-x64"
    property string customization: "default"
    property bool beta: true

    property bool withInstallers: false

    property path buildenvDirectory: Environment.getEnv("environment")

    references: [
        (qbs.versionMajor >= 1 && qbs.versionMinor >= 8
            ? "qbs/dependencies.qbs" : "qbs/dependencies_17.qbs"),
        "qbs/mercurial_info.qbs",
        "common_libs",
        "common",
        "appserver2",
        "nx_cloud",
        "client",
        "mediaserver_core",
        "mediaserver",
        "plugins",
        "debsetup"
    ]
}
